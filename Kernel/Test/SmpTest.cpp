#include "Test/Test.h"
#include "Arch/ControlRegister.h"
#include "Arch/Cpu.h"
#include "Interrupt/Guard.h"
#include "Sync/Semaphore.h"
#include "Thread/Thread.h"

extern "C" uint64_t KernelPageTableRoot;

namespace {

constexpr unsigned WORKER_COUNT = 4;
constexpr unsigned SAMPLES = 2000;

Semaphore s_Finished(0);

/// One bit per core that actually executed a thread. If the application
/// processors failed to come up on the kernel's page tables, only the
/// bootstrap core's bit is ever set.
uint64_t s_CoresSeen = 0;

/// Same bits, but only for cores whose CR3 is the kernel's own root. An
/// application processor that missed the handover would still run, on the boot
/// map, and show up here as a hole.
uint64_t s_CoresOnKernelTables = 0;

class SampleThread : public Thread {
public:
    void Action() override {
        for (unsigned round = 0; round < SAMPLES; round++) {
            const uint64_t bit = uint64_t{1} << Cpu::GetId();
            __atomic_or_fetch(&s_CoresSeen, bit, __ATOMIC_RELAXED);

            if ((Cpu::CR3::Read() & ~uintptr_t{0xfff}) == KernelPageTableRoot) {
                __atomic_or_fetch(&s_CoresOnKernelTables, bit, __ATOMIC_RELAXED);
            }
        }

        Guarded guard = Guard::Enter();
        s_Finished.V(guard.Vault());
    }
};

SampleThread s_Workers[WORKER_COUNT];

unsigned CountBits(uint64_t InValue) {
    unsigned count = 0;
    for (; InValue != 0; InValue >>= 1) {
        count += InValue & 1;
    }
    return count;
}

}  // namespace

void Test::RunSmpSuite() {
    Begin("Smp");

    {
        Guarded guard = Guard::Enter();
        for (SampleThread& worker : s_Workers) {
            guard.Vault().Scheduler.Ready(&worker);
        }
    }

    for (unsigned index = 0; index < WORKER_COUNT; index++) {
        Guarded guard = Guard::Enter();
        s_Finished.P(guard.Vault());
    }

    const uint64_t seenMask = __atomic_load_n(&s_CoresSeen, __ATOMIC_RELAXED);
    const unsigned seen = CountBits(seenMask);
    TEST_CHECK(seen >= 1);

    TEST_CHECK_EQ(__atomic_load_n(&s_CoresOnKernelTables, __ATOMIC_RELAXED), seenMask);

    // Every core the firmware reported should have run something by now.
    if (Cpu::Count() > 1) {
        TEST_CHECK(seen > 1);
    }
    TEST_CHECK(seen <= Cpu::Count());
}
