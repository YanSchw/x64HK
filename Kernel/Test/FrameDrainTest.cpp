#include "Test/Test.h"
#include "Boot/Multiboot.h"
#include "Config.h"
#include "Memory/Frame.h"
#include "Memory/Paging.h"

namespace {

struct Block {
    uintptr_t Address;
    size_t Count;
};

/// Large enough that the halving below always runs out of memory before it
/// runs out of slots.
Block s_Blocks[1024];
size_t s_BlockCount = 0;

constexpr size_t LARGEST_CHUNK = 4096;

bool HasMemoryAboveBootMap() {
    Multiboot::MemoryRegion region;
    for (unsigned index = 0; Multiboot::GetMemoryRegion(index, region); index++) {
        if (region.Type == Multiboot::MemoryType::AVAILABLE &&
            region.End() > Config::IDENTITY_MAPPED_LIMIT) {
            return true;
        }
    }
    return false;
}

bool Record(uintptr_t InAddress, size_t InCount) {
    if (s_BlockCount == ArraySize(s_Blocks)) {
        return false;
    }
    s_Blocks[s_BlockCount++] = {InAddress, InCount};
    return true;
}

}  // namespace

void Test::RunFrameDrainSuite() {
    Begin("Frame drain");

    const size_t free = Frame::GetFreeFrames();
    size_t taken = 0;

    // Halving down to single frames picks up the remainders that the larger
    // runs leave behind, so the allocator really does end up empty.
    for (size_t chunk = LARGEST_CHUNK; chunk >= 1; chunk /= 2) {
        while (true) {
            const uintptr_t block = Frame::Allocate(chunk);
            if (block == 0 || !Record(block, chunk)) {
                break;
            }
            taken += chunk;
        }
    }

    TEST_CHECK_EQ(taken, free);
    TEST_CHECK_EQ(Frame::GetFreeFrames(), size_t{0});
    TEST_CHECK_EQ(Frame::Allocate(), uintptr_t{0});

    // Holding every frame is the cheapest chance to prove that the ones past
    // the boot map's 4 GiB are real memory and not just accounting.
    unsigned highBlocks = 0;
    unsigned highFailures = 0;
    for (size_t index = 0; index < s_BlockCount; index++) {
        const uintptr_t address = s_Blocks[index].Address;
        if (address < Config::IDENTITY_MAPPED_LIMIT) {
            continue;
        }

        highBlocks++;
        auto* probe = reinterpret_cast<volatile uint64_t*>(Paging::ToVirtual(address));
        *probe = address;
        if (*probe != address) {
            highFailures++;
        }
    }
    TEST_CHECK_EQ(highFailures, 0u);
    TEST_CHECK_EQ(highBlocks > 0, HasMemoryAboveBootMap());

    for (size_t index = 0; index < s_BlockCount; index++) {
        Frame::Free(s_Blocks[index].Address, s_Blocks[index].Count);
    }

    TEST_CHECK_EQ(Frame::GetFreeFrames(), free);

    const uintptr_t reused = Frame::Allocate();
    TEST_CHECK(reused != 0);
    Frame::Free(reused);
    TEST_CHECK_EQ(Frame::GetFreeFrames(), free);
}
