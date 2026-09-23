#include "Test/Test.h"
#include "Boot/Multiboot.h"
#include "Config.h"
#include "Memory/Frame.h"
#include "Memory/Paging.h"

extern "C" uint8_t ___BOOT_START___;
extern "C" uint8_t ___KERNEL_END___;

namespace {

bool IsInAvailableRegion(uintptr_t InAddress) {
    Multiboot::MemoryRegion region;
    for (unsigned index = 0; Multiboot::GetMemoryRegion(index, region); index++) {
        if (region.Type != Multiboot::MemoryType::AVAILABLE) {
            continue;
        }
        if (InAddress >= region.Address && InAddress + Frame::SIZE <= region.End()) {
            return true;
        }
    }
    return false;
}

bool OverlapsKernel(uintptr_t InAddress) {
    const uintptr_t start = reinterpret_cast<uintptr_t>(&___BOOT_START___);
    const uintptr_t end = reinterpret_cast<uintptr_t>(&___KERNEL_END___) - Config::KERNEL_VMA;
    return InAddress < end && InAddress + Frame::SIZE > start;
}

bool IsUsable(uintptr_t InAddress) {
    return InAddress >= MIB && InAddress % Frame::SIZE == 0 && !OverlapsKernel(InAddress) &&
           IsInAvailableRegion(InAddress);
}

/// Frames come back as physical addresses; only the direct map can reach them.
volatile uint64_t* Contents(uintptr_t InFrame) {
    return reinterpret_cast<volatile uint64_t*>(Paging::ToVirtual(InFrame));
}

void Stamp(uintptr_t InFrame, uint64_t InValue) {
    *Contents(InFrame) = InValue;
}

uint64_t Read(uintptr_t InFrame) {
    return *Contents(InFrame);
}

}  // namespace

void Test::RunFrameSuite() {
    Begin("Frame");

    const size_t total = Frame::GetTotalFrames();
    const size_t free = Frame::GetFreeFrames();

    TEST_CHECK(total > 0);
    TEST_CHECK(free > 0);
    TEST_CHECK_EQ(Frame::GetUsedFrames(), total - free);

    // Everything the loader called available is managed, give or take the
    // partial frames at each region's edges.
    const uint64_t available = Multiboot::GetAvailableMemory();
    const uint64_t managed = uint64_t{total} * Frame::SIZE;
    const uint64_t slack = uint64_t{Multiboot::GetMemoryRegionCount()} * 2 * Frame::SIZE;
    TEST_CHECK(managed <= available);
    TEST_CHECK(available - managed <= slack);

    // The heap took its region during boot, so something must be in use.
    TEST_CHECK(Frame::GetUsedFrames() > 0);

    TEST_CHECK_EQ(Frame::Allocate(0), uintptr_t{0});
    Frame::Free(0);
    TEST_CHECK_EQ(Frame::GetFreeFrames(), free);

    const uintptr_t single = Frame::Allocate();
    TEST_CHECK(single != 0);
    TEST_CHECK(IsUsable(single));
    TEST_CHECK_EQ(Frame::GetFreeFrames(), free - 1);
    Frame::Free(single);
    TEST_CHECK_EQ(Frame::GetFreeFrames(), free);

    // Every frame has to be handed out to one caller at a time. Stamping each
    // one and reading it back afterwards catches any that alias.
    constexpr size_t BATCH = 256;
    uintptr_t frames[BATCH] = {};
    unsigned unusable = 0;
    unsigned aliased = 0;

    for (size_t index = 0; index < BATCH; index++) {
        frames[index] = Frame::Allocate();
        if (!IsUsable(frames[index])) {
            unusable++;
        }
        Stamp(frames[index], index + 1);
    }
    for (size_t index = 0; index < BATCH; index++) {
        if (Read(frames[index]) != index + 1) {
            aliased++;
        }
    }

    TEST_CHECK_EQ(unusable, 0u);
    TEST_CHECK_EQ(aliased, 0u);
    TEST_CHECK_EQ(Frame::GetFreeFrames(), free - BATCH);

    for (size_t index = 0; index < BATCH; index++) {
        Frame::Free(frames[index]);
    }
    TEST_CHECK_EQ(Frame::GetFreeFrames(), free);

    // A contiguous run has to be genuinely contiguous and genuinely backed.
    constexpr size_t RUN = 16;
    const uintptr_t block = Frame::Allocate(RUN);
    TEST_CHECK(block != 0);
    TEST_CHECK(IsUsable(block));
    TEST_CHECK_EQ(Frame::GetFreeFrames(), free - RUN);

    unsigned corrupted = 0;
    for (size_t index = 0; index < RUN; index++) {
        Stamp(block + index * Frame::SIZE, index + 1);
    }
    for (size_t index = 0; index < RUN; index++) {
        if (Read(block + index * Frame::SIZE) != index + 1) {
            corrupted++;
        }
        if (!IsUsable(block + index * Frame::SIZE)) {
            corrupted++;
        }
    }
    TEST_CHECK_EQ(corrupted, 0u);

    Frame::Free(block, RUN);
    TEST_CHECK_EQ(Frame::GetFreeFrames(), free);

    // Asking for more than exists fails instead of wrapping or overcommitting.
    TEST_CHECK_EQ(Frame::Allocate(total + 1), uintptr_t{0});
    TEST_CHECK_EQ(Frame::GetFreeFrames(), free);
}
