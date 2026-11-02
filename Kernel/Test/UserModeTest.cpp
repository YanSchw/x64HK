#include "Test/Test.h"
#include "ABI/C/HK.h"
#include "Config.h"
#include "Interrupt/Guard.h"
#include "Lib/String.h"
#include "Memory/Frame.h"
#include "Device/Key.h"
#include "Memory/Paging.h"
#include "Thread/Thread.h"
#include "User/Task.h"

// Every rule ring 3 has to obey, checked by breaking it.
//
// Each probe is a handful of instructions from Test/UserProbes.asm, copied into
// a user page and run as a task. The suite then waits for the task to die and
// looks at the status it died with -- HK_EXIT_FAULT_BASE plus the vector for a
// fault, or whatever the probe passed to Exit.

extern "C" uint8_t ___KERNEL_TEXT_START___;

extern "C" {
#define PROBE_SYMBOLS(NAME)      \
    extern uint8_t Probe##NAME##Start; \
    extern uint8_t Probe##NAME##End;

PROBE_SYMBOLS(Exit)
PROBE_SYMBOLS(Yield)
PROBE_SYMBOLS(GetKey)
PROBE_SYMBOLS(UnknownCall)
PROBE_SYMBOLS(KernelWrite)
PROBE_SYMBOLS(ReservedArgument)
PROBE_SYMBOLS(Cli)
PROBE_SYMBOLS(Hlt)
PROBE_SYMBOLS(Out)
PROBE_SYMBOLS(Interrupt)
PROBE_SYMBOLS(KernelRead)
PROBE_SYMBOLS(DirectMapRead)
PROBE_SYMBOLS(WriteText)
PROBE_SYMBOLS(ExecuteStack)
PROBE_SYMBOLS(Divide)
#undef PROBE_SYMBOLS
}

namespace {

constexpr int FAULT(unsigned InVector) {
    return HK_EXIT_FAULT_BASE + static_cast<int>(InVector);
}

constexpr int DIVIDE_ERROR = FAULT(0);
constexpr int INVALID_OPCODE = FAULT(6);
constexpr int PROTECTION_FAULT = FAULT(13);
constexpr int PAGE_FAULT = FAULT(14);

struct Probe {
    const char* Name;
    const uint8_t* Code;
    const uint8_t* CodeEnd;
    int ExpectedStatus;

    /// Hand the Vault a keystroke once the probe is running.
    bool FeedKey = false;

    /// Check afterwards that the kernel left the user stack alone.
    bool CheckStack = false;
};

#define PROBE(NAME, EXPECTED) \
    {#NAME, &Probe##NAME##Start, &Probe##NAME##End, (EXPECTED)}

/// The suite hands this one over, so its encoding is what GetKey has to return.
constexpr Key::ScanCode FED_KEY = Key::ScanCode::A;

const Probe PROBES[] = {
    PROBE(Exit, 7),
    {"Yield", &ProbeYieldStart, &ProbeYieldEnd, 0, false, true},
    {"GetKey", &ProbeGetKeyStart, &ProbeGetKeyEnd,
     static_cast<int>(hk_key_pack(ToUnderlying(FED_KEY), 'a', 0)), true, false},
    PROBE(UnknownCall, -HK_ENOSYS),
    PROBE(KernelWrite, -HK_EFAULT),
    PROBE(ReservedArgument, -HK_EINVAL),
    PROBE(Cli, PROTECTION_FAULT),
    PROBE(Hlt, PROTECTION_FAULT),
    PROBE(Out, PROTECTION_FAULT),
    PROBE(Interrupt, PROTECTION_FAULT),
    PROBE(KernelRead, PAGE_FAULT),
    PROBE(DirectMapRead, PAGE_FAULT),
    PROBE(WriteText, PAGE_FAULT),
    PROBE(ExecuteStack, PAGE_FAULT),
    PROBE(Divide, DIVIDE_ERROR),
};
#undef PROBE

constexpr size_t PROBE_COUNT = ArraySize(PROBES);

/// Clear of the demo program's link address, which the test build does not load
/// but which the suite has no business colliding with either.
constexpr uintptr_t PROBE_BASE = 0x1000'0000;

User::Task s_Tasks[PROBE_COUNT];
Thread s_Threads[PROBE_COUNT];

/// Copies a probe into a frame and maps it where ring 3 can execute it. The
/// copy goes through the direct map, so the page is never writable from below.
uintptr_t MapProbe(const Probe& InProbe, unsigned InIndex) {
    const uintptr_t frame = Frame::Allocate();
    if (frame == 0) {
        return 0;
    }

    uint8_t* target = reinterpret_cast<uint8_t*>(Paging::ToVirtual(frame));
    memset(target, 0, Paging::PAGE_SIZE);
    memcpy(target, InProbe.Code, static_cast<size_t>(InProbe.CodeEnd - InProbe.Code));

    const uintptr_t page = PROBE_BASE + InIndex * Paging::PAGE_SIZE;
    if (!Paging::Map(page, frame, Paging::PAGE_SIZE, Paging::Access::READ_EXECUTE,
                     Paging::Privilege::USER)) {
        return 0;
    }

    return page;
}

void Sleep(unsigned InMilliseconds) {
    Guarded guard = Guard::Enter();
    guard.Vault().Bellringer.Sleep(guard.Vault(), InMilliseconds);
}

/// Exactly what the keyboard epilogue does, minus the keyboard.
void FeedKey() {
    Guarded guard = Guard::Enter();
    Vault& vault = guard.Vault();

    Key key;
    key.Code = FED_KEY;
    if (vault.Keys.Produce(key)) {
        vault.KeysAvailable.V(vault);
    }
}

/// Nothing the kernel does may land on the user stack: an interrupt taken in
/// ring 3 switches to the TSS stack and a system call switches to the same one
/// by hand. The stack starts zeroed and these probes never push, so a non-zero
/// byte below the entry rsp means one of those two went wrong.
bool StackIsUntouched(uintptr_t InStackTop) {
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(InStackTop);
    for (size_t offset = 1; offset <= 256; offset++) {
        if (bytes[-static_cast<ptrdiff_t>(offset)] != 0) {
            return false;
        }
    }
    return true;
}

/// The runner has nothing to join on, so it sleeps until the task count moves.
bool WaitForTermination(unsigned InExpected) {
    for (unsigned attempt = 0; attempt < 400; attempt++) {
        if (User::LastTermination().Count >= InExpected) {
            return true;
        }

        Guarded guard = Guard::Enter();
        guard.Vault().Bellringer.Sleep(guard.Vault(), 5);
    }
    return false;
}

}  // namespace

void Test::RunUserModeSuite() {
    Begin("User mode");

    // The probes that read kernel memory are only worth anything if the
    // addresses they pick are mapped. Otherwise they would fault for being
    // absent and say nothing about the U/S bit.
    const uintptr_t kernelText = reinterpret_cast<uintptr_t>(&___KERNEL_TEXT_START___);
    const uintptr_t directMap = Config::DIRECT_MAP_BASE + 0x1000;

    TEST_CHECK(Paging::Translate(kernelText) != 0);
    TEST_CHECK(!Paging::IsUserReadable(kernelText));
    TEST_CHECK(Paging::Translate(directMap) != 0);
    TEST_CHECK(!Paging::IsUserReadable(directMap));

    // The probe that runs off its stack only means something with the bit that
    // makes it fault.
    const bool noExecute = !Paging::IsExecutable(Config::KERNEL_STACK_BASE + Paging::PAGE_SIZE);

    for (unsigned index = 0; index < PROBE_COUNT; index++) {
        const Probe& probe = PROBES[index];

        if (probe.Code == &ProbeExecuteStackStart && !noExecute) {
            continue;
        }

        const uintptr_t entry = MapProbe(probe, index);
        TEST_CHECK(entry != 0);
        if (entry == 0) {
            continue;
        }

        s_Tasks[index] = User::Task{probe.Name, entry, User::AllocateStack()};
        TEST_CHECK(s_Tasks[index].StackTop != 0);

        const unsigned before = User::LastTermination().Count;

        s_Threads[index].Prepare(probe.Name, User::Run, &s_Tasks[index]);
        {
            Guarded guard = Guard::Enter();
            guard.Vault().Scheduler.Ready(&s_Threads[index]);
        }

        if (probe.FeedKey) {
            // Long enough for the probe to have reached GetKey and blocked. If
            // it has not, the key simply waits in the buffer and the semaphore
            // count carries it, so this is a timing convenience, not a race.
            Sleep(50);
            FeedKey();
        }

        if (!WaitForTermination(before + 1)) {
            Out() << "          " << probe.Name << " never died" << EndLine << Flush;
            TEST_CHECK(false);
            continue;
        }

        TEST_CHECK_EQ(User::LastTermination().Status, probe.ExpectedStatus);

        if (probe.CheckStack) {
            TEST_CHECK(StackIsUntouched(s_Tasks[index].StackTop));
        }
    }

    // The whole point: twelve ring 3 programs have just been killed and the
    // kernel is still the one asking the questions.
    TEST_CHECK(User::LastTermination().Count >= PROBE_COUNT - 1);
}
