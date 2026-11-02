#include "User/Task.h"
#include "ABI/C/HK.h"
#include "Arch/Cpu.h"
#include "Arch/Syscall.h"
#include "Debug/Output.h"
#include "Debug/Panic.h"
#include "Interrupt/Guard.h"
#include "Lib/Math.h"
#include "Lib/String.h"
#include "Memory/Frame.h"
#include "Memory/Paging.h"
#include "Sync/SpinLock.h"
#include "Thread/Thread.h"
#include "User/Elf.h"

namespace User {

constexpr size_t PAGE_SIZE = Paging::PAGE_SIZE;
constexpr size_t GUARD_SIZE = PAGE_SIZE;
constexpr size_t STACK_STRIDE = HK_USER_STACK_SIZE + GUARD_SIZE;

// Stacks are handed out downwards from the top of the user half. Tasks are
// never destroyed, so a moving watermark is enough.
static uintptr_t s_NextStackTop = HK_USER_STACK_TOP;
static SpinLock s_StackLock;

uintptr_t AllocateStack() {
    uintptr_t top = 0;
    {
        Cpu::Interrupt::Guard interruptGuard;
        SpinLock::Scope lockGuard(s_StackLock);
        top = s_NextStackTop;
        s_NextStackTop -= STACK_STRIDE;
    }

    const uintptr_t frames = Frame::Allocate(HK_USER_STACK_SIZE / PAGE_SIZE);
    if (frames == 0) {
        return 0;
    }

    const uintptr_t bottom = top - HK_USER_STACK_SIZE;
    memset(reinterpret_cast<void*>(Paging::ToVirtual(frames)), 0, HK_USER_STACK_SIZE);

    // The guard is the page below, and is simply never mapped.
    if (!Paging::Map(bottom, frames, HK_USER_STACK_SIZE, Paging::Access::READ_WRITE,
                     Paging::Privilege::USER)) {
        return 0;
    }

    return top;
}

static Paging::Access AccessFor(uint32_t InFlags) {
    if ((InFlags & Elf::PF_EXECUTE) != 0) {
        return Paging::Access::READ_EXECUTE;
    }
    if ((InFlags & Elf::PF_WRITE) != 0) {
        return Paging::Access::READ_WRITE;
    }
    return Paging::Access::READ_ONLY;
}

static bool InUserHalf(uint64_t InStart, uint64_t InLength) {
    const uint64_t end = InStart + InLength;
    return InStart >= HK_USER_MIN && end >= InStart && end <= HK_USER_MAX;
}

/// Frames for one segment, zeroed and then filled from the image. Zeroing first
/// is what gives .bss its guarantee, and it also keeps whatever the frame held
/// before out of ring 3.
static bool LoadSegment(const uint8_t* InImage, const Elf::ProgramHeader& InHeader) {
    const uintptr_t start = InHeader.VirtualAddress & ~(PAGE_SIZE - 1);
    const uintptr_t end = Math::AlignUp<uint64_t>(InHeader.VirtualAddress + InHeader.MemorySize,
                                                  PAGE_SIZE);
    const size_t size = end - start;

    const uintptr_t frames = Frame::Allocate(size / PAGE_SIZE);
    if (frames == 0) {
        return false;
    }

    uint8_t* target = reinterpret_cast<uint8_t*>(Paging::ToVirtual(frames));
    memset(target, 0, size);
    memcpy(target + (InHeader.VirtualAddress - start), InImage + InHeader.Offset,
           InHeader.FileSize);

    return Paging::Map(start, frames, size, AccessFor(InHeader.Flags), Paging::Privilege::USER);
}

static bool IsRunnable(const Elf::Header& InHeader, size_t InSize) {
    return InSize >= sizeof(Elf::Header) && InHeader.Magic[0] == 0x7f && InHeader.Magic[1] == 'E' &&
           InHeader.Magic[2] == 'L' && InHeader.Magic[3] == 'F' &&
           InHeader.Class == Elf::CLASS_64 && InHeader.Data == Elf::DATA_LITTLE_ENDIAN &&
           InHeader.Type == Elf::TYPE_EXECUTABLE && InHeader.Machine == Elf::MACHINE_X86_64 &&
           InHeader.ProgramHeaderEntrySize == sizeof(Elf::ProgramHeader) &&
           InHeader.ProgramHeaderCount > 0;
}

bool Load(const char* InName, const void* InImage, size_t InSize, Task& OutTask) {
    const uint8_t* image = static_cast<const uint8_t*>(InImage);
    const Elf::Header& header = *reinterpret_cast<const Elf::Header*>(image);

    if (!IsRunnable(header, InSize)) {
        DBG << "User: '" << InName << "' is not a 64-bit x86 executable" << EndLine;
        return false;
    }

    for (unsigned index = 0; index < header.ProgramHeaderCount; index++) {
        const Elf::ProgramHeader& segment = *reinterpret_cast<const Elf::ProgramHeader*>(
            image + header.ProgramHeaderOffset + index * sizeof(Elf::ProgramHeader));

        if (segment.Type != Elf::PT_LOAD || segment.MemorySize == 0) {
            continue;
        }

        // Writable and executable at once is exactly what the Access enum has no
        // way to express, so an image asking for it is rejected rather than
        // quietly downgraded.
        if ((segment.Flags & Elf::PF_WRITE) != 0 && (segment.Flags & Elf::PF_EXECUTE) != 0) {
            DBG << "User: '" << InName << "' wants a writable code segment" << EndLine;
            return false;
        }
        if (segment.Offset + segment.FileSize > InSize ||
            segment.FileSize > segment.MemorySize ||
            !InUserHalf(segment.VirtualAddress, segment.MemorySize)) {
            DBG << "User: '" << InName << "' has a segment outside the user half" << EndLine;
            return false;
        }
        if (!LoadSegment(image, segment)) {
            DBG << "User: out of memory loading '" << InName << "'" << EndLine;
            return false;
        }
    }

    if (!InUserHalf(header.Entry, 1) || !Paging::IsExecutable(header.Entry)) {
        DBG << "User: '" << InName << "' has no executable entry point" << EndLine;
        return false;
    }

    OutTask.Name = InName;
    OutTask.Entry = header.Entry;
    OutTask.StackTop = AllocateStack();

    return OutTask.StackTop != 0;
}

void Run(void* InTask) {
    const Task& task = *static_cast<const Task*>(InTask);
    DBG << "User: '" << task.Name << "' entering ring 3 at " << Hex
        << reinterpret_cast<void*>(task.Entry) << Dec << EndLine;
    EnterUserMode(task.Entry, task.StackTop);
}

static unsigned s_TerminationCount = 0;
static int s_LastStatus = 0;

Termination LastTermination() {
    // The count is the release, so a reader that sees it has the status too.
    const unsigned count = __atomic_load_n(&s_TerminationCount, __ATOMIC_ACQUIRE);
    return {count, __atomic_load_n(&s_LastStatus, __ATOMIC_RELAXED)};
}

void Terminate(const char* InReason, int InStatus) {
    const Thread* thread = Guard::UnsafeVault().Scheduler.Active();

    DBG << "User: '" << (thread != nullptr ? thread->Name() : "?") << "' " << InReason
        << ", status " << Dec << InStatus << " on core " << Cpu::GetId() << EndLine;

    __atomic_store_n(&s_LastStatus, InStatus, __ATOMIC_RELAXED);
    __atomic_add_fetch(&s_TerminationCount, 1, __ATOMIC_RELEASE);

    // Exit hands the critical section to whoever runs next, exactly as a thread
    // that returns from its body does.
    Guard::Enter().Vault().Scheduler.Exit();
    PANIC("Scheduler returned into a terminated user task");
}

bool CopyIn(void* OutDestination, uintptr_t InSource, size_t InLength) {
    if (InLength == 0) {
        return true;
    }
    if (!InUserHalf(InSource, InLength)) {
        return false;
    }

    // Nothing is ever unmapped, so a page that reads as user accessible here is
    // still user accessible at the memcpy below.
    for (uintptr_t page = InSource & ~(PAGE_SIZE - 1); page < InSource + InLength;
         page += PAGE_SIZE) {
        if (!Paging::IsUserReadable(page)) {
            return false;
        }
    }

    memcpy(OutDestination, reinterpret_cast<const void*>(InSource), InLength);
    return true;
}

}  // namespace User
