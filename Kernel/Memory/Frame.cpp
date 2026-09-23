#include "Memory/Frame.h"
#include "Arch/CpuInterrupt.h"
#include "Boot/Multiboot.h"
#include "Config.h"
#include "Debug/Assert.h"
#include "Debug/Output.h"
#include "Debug/Panic.h"
#include "Lib/Math.h"
#include "Sync/SpinLock.h"

extern "C" uint8_t ___BOOT_START___;
extern "C" uint8_t ___KERNEL_END___;

namespace Frame {

constexpr size_t BITS_PER_WORD = 64;
constexpr uint64_t ALL_TAKEN = ~uint64_t{0};

/// A set bit is a frame that must not be handed out. Everything starts set, so
/// only memory that is explicitly released can ever be allocated.
static uint64_t* s_Taken = nullptr;
static size_t s_WordCount = 0;
static size_t s_FrameCount = 0;

/// Frames past this are managed but never handed out: nothing maps them yet.
static size_t s_ReachableFrames = 0;

static SpinLock s_Lock;
static size_t s_TotalFrames = 0;
static size_t s_FreeFrames = 0;

static size_t s_NextIndex = 0;
constexpr size_t NOT_FOUND = SIZE_MAX;

static bool IsTaken(size_t InIndex) {
    return (s_Taken[InIndex / BITS_PER_WORD] & (uint64_t{1} << (InIndex % BITS_PER_WORD))) != 0;
}

static void Take(size_t InIndex) {
    s_Taken[InIndex / BITS_PER_WORD] |= uint64_t{1} << (InIndex % BITS_PER_WORD);
}

static void Release(size_t InIndex) {
    s_Taken[InIndex / BITS_PER_WORD] &= ~(uint64_t{1} << (InIndex % BITS_PER_WORD));
}

/// Whole frames inside [InStart, InEnd) become allocatable.
static void Add(uint64_t InStart, uint64_t InEnd) {
    const size_t first = (InStart + SIZE - 1) / SIZE;
    const size_t last = Math::Min<uint64_t>(InEnd / SIZE, s_FrameCount);

    for (size_t index = first; index < last; index++) {
        if (IsTaken(index)) {
            Release(index);
            s_TotalFrames++;
            s_FreeFrames++;
        }
    }
}

/// Every frame [InStart, InEnd) touches is withdrawn, including partial ones.
static void Reserve(uint64_t InStart, uint64_t InEnd) {
    if (InEnd <= InStart) {
        return;
    }

    const size_t first = InStart / SIZE;
    const size_t last = Math::Min<uint64_t>((InEnd + SIZE - 1) / SIZE, s_FrameCount);

    for (size_t index = first; index < last; index++) {
        if (!IsTaken(index)) {
            Take(index);
            s_FreeFrames--;
        }
    }
}

struct Range {
    uint64_t Start;
    uint64_t End;
};

static Range KernelRange() {
    return {reinterpret_cast<uintptr_t>(&___BOOT_START___),
            reinterpret_cast<uintptr_t>(&___KERNEL_END___) - Config::KERNEL_VMA};
}

static uint64_t HighestAvailableAddress() {
    uint64_t highest = 0;

    Multiboot::MemoryRegion region;
    for (unsigned index = 0; Multiboot::GetMemoryRegion(index, region); index++) {
        if (region.Type == Multiboot::MemoryType::AVAILABLE) {
            highest = Math::Max(highest, region.End());
        }
    }

    return highest;
}

static bool Overlaps(Range InLeft, Range InRight) {
    return InLeft.Start < InRight.End && InRight.Start < InLeft.End;
}

static bool IsBlocked(Range InRange, uint64_t& OutRetryAt) {
    if (Overlaps(InRange, KernelRange())) {
        OutRetryAt = KernelRange().End;
        return true;
    }

    Multiboot::MemoryRegion info;
    for (unsigned index = 0; Multiboot::GetInfoRegion(index, info); index++) {
        if (info.Length != 0 && Overlaps(InRange, {info.Address, info.End()})) {
            OutRetryAt = info.End();
            return true;
        }
    }

    Multiboot::Module module;
    for (unsigned index = 0; Multiboot::GetModule(index, module); index++) {
        if (Overlaps(InRange, {module.Start, module.End})) {
            OutRetryAt = module.End;
            return true;
        }
    }

    return false;
}

static uintptr_t FirstFit(Range InRange, size_t InBytes) {
    uint64_t start = (InRange.Start + SIZE - 1) & ~uint64_t{SIZE - 1};

    while (start + InBytes <= InRange.End) {
        uint64_t retryAt = 0;
        if (!IsBlocked({start, start + InBytes}, retryAt)) {
            return start;
        }
        start = (retryAt + SIZE - 1) & ~uint64_t{SIZE - 1};
    }

    return 0;
}

static uintptr_t ChooseBitmapLocation(size_t InBytes) {
    Multiboot::MemoryRegion region;
    for (unsigned index = 0; Multiboot::GetMemoryRegion(index, region); index++) {
        if (region.Type != Multiboot::MemoryType::AVAILABLE) {
            continue;
        }

        const Range usable = {Math::Max<uint64_t>(region.Address, MIB),
                              Math::Min<uint64_t>(region.End(), Config::IDENTITY_MAPPED_LIMIT)};
        if (usable.Start >= usable.End) {
            continue;
        }

        const uintptr_t location = FirstFit(usable, InBytes);
        if (location != 0) {
            return location;
        }
    }

    return 0;
}

static void AddAvailableMemory() {
    Multiboot::MemoryRegion region;
    for (unsigned index = 0; Multiboot::GetMemoryRegion(index, region); index++) {
        if (region.Type == Multiboot::MemoryType::AVAILABLE) {
            Add(region.Address, region.End());
        }
    }
}

static void ReserveKnownUsers() {
    // BIOS data, the EBDA, video memory, and the page the application
    // processors start on.
    Reserve(0, MIB);

    const Range kernel = KernelRange();
    Reserve(kernel.Start, kernel.End);

    Multiboot::MemoryRegion info;
    for (unsigned index = 0; Multiboot::GetInfoRegion(index, info); index++) {
        Reserve(info.Address, info.End());
    }

    Multiboot::Module module;
    for (unsigned index = 0; Multiboot::GetModule(index, module); index++) {
        Reserve(module.Start, module.End);
    }
}

void Initialize() {
    s_FrameCount = (HighestAvailableAddress() + SIZE - 1) / SIZE;
    s_WordCount = (s_FrameCount + BITS_PER_WORD - 1) / BITS_PER_WORD;

    const size_t bytes = s_WordCount * sizeof(uint64_t);
    const uintptr_t location = ChooseBitmapLocation(bytes);
    if (location == 0) {
        PANIC("No room for the frame bitmap");
    }
    s_Taken = reinterpret_cast<uint64_t*>(location + Config::DIRECT_MAP_BASE);

    for (size_t index = 0; index < s_WordCount; index++) {
        s_Taken[index] = ALL_TAKEN;
    }
    s_TotalFrames = 0;
    s_FreeFrames = 0;
    s_NextIndex = 0;
    s_ReachableFrames = Math::Min<uint64_t>(Config::IDENTITY_MAPPED_LIMIT / SIZE, s_FrameCount);

    AddAvailableMemory();
    ReserveKnownUsers();
    Reserve(location, location + bytes);

    if (s_FreeFrames == 0) {
        PANIC("No usable physical memory");
    }
}

void SetReachableLimit(uintptr_t InLimit) {
    Cpu::Interrupt::Guard interruptGuard;
    SpinLock::Scope lockGuard(s_Lock);

    s_ReachableFrames = Math::Min<uint64_t>(InLimit / SIZE, s_FrameCount);
}

static size_t FindRun(size_t InFrom, size_t InCount) {
    size_t run = 0;

    for (size_t index = InFrom; index < s_ReachableFrames; index++) {
        if (IsTaken(index)) {
            run = 0;
            continue;
        }
        if (++run == InCount) {
            return index + 1 - InCount;
        }
    }

    return NOT_FOUND;
}

uintptr_t Allocate(size_t InCount) {
    if (InCount == 0) {
        return 0;
    }

    Cpu::Interrupt::Guard interruptGuard;
    SpinLock::Scope lockGuard(s_Lock);

    size_t first = FindRun(s_NextIndex, InCount);
    if (first == NOT_FOUND && s_NextIndex != 0) {
        first = FindRun(0, InCount);
    }
    if (first == NOT_FOUND) {
        return 0;
    }

    for (size_t index = first; index < first + InCount; index++) {
        Take(index);
    }
    s_FreeFrames -= InCount;
    s_NextIndex = first + InCount < s_ReachableFrames ? first + InCount : 0;

    return first * SIZE;
}

void Free(uintptr_t InAddress, size_t InCount) {
    if (InAddress == 0 || InCount == 0) {
        return;
    }
    ASSERT(InAddress % SIZE == 0);

    Cpu::Interrupt::Guard interruptGuard;
    SpinLock::Scope lockGuard(s_Lock);

    const size_t first = InAddress / SIZE;
    ASSERT(first + InCount <= s_FrameCount);

    for (size_t index = first; index < first + InCount; index++) {
        ASSERT(IsTaken(index));
        Release(index);
        s_FreeFrames++;
    }
}

size_t GetTotalFrames() {
    return s_TotalFrames;
}

size_t GetFreeFrames() {
    return s_FreeFrames;
}

size_t GetUsedFrames() {
    return s_TotalFrames - s_FreeFrames;
}

void Dump() {
    const uint64_t managed = uint64_t{s_TotalFrames} * SIZE;
    const uint64_t available = Multiboot::GetAvailableMemory();

    DBG << "Frames: " << Dec << managed / MIB << " MiB managed, " << GetUsedFrames() * SIZE / MIB
        << " MiB used" << EndLine;

    if (available - managed >= MIB) {
        DBG << "  " << Dec << (available - managed) / MIB << " MiB unusable" << EndLine;
    }
}

}  // namespace Frame
