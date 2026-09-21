#include "Test/Test.h"
#include "Memory/Heap.h"

void Test::RunHeapSuite() {
    Begin("Heap");

    const size_t baseline = Heap::GetUsedBytes();
    TEST_CHECK(Heap::GetTotalBytes() > 0);

    void* first = Heap::Allocate(64);
    void* second = Heap::Allocate(64);
    TEST_CHECK(first != nullptr);
    TEST_CHECK(second != nullptr);
    TEST_CHECK(first != second);

    // The buddy allocator's smallest block is sized so every payload lands on a
    // 16 byte boundary; SSE would fault on anything less even though we never
    // use it.
    TEST_CHECK((reinterpret_cast<uintptr_t>(first) & 0xf) == 0);
    TEST_CHECK((reinterpret_cast<uintptr_t>(second) & 0xf) == 0);

    Heap::Free(first);
    Heap::Free(second);
    TEST_CHECK_EQ(Heap::GetUsedBytes(), baseline);

    // Freeing a null pointer is allowed and has to stay a no-op.
    Heap::Free(nullptr);
    TEST_CHECK_EQ(Heap::GetUsedBytes(), baseline);

    // Writing the whole payload catches a block that overlaps its neighbour.
    unsigned failures = 0;
    for (unsigned round = 0; round < 512; round++) {
        const size_t size = 16 + (round * 7) % 2048;
        uint8_t* block = static_cast<uint8_t*>(Heap::Allocate(size));
        if (block == nullptr) {
            failures++;
            continue;
        }
        for (size_t index = 0; index < size; index++) {
            block[index] = static_cast<uint8_t>(index);
        }
        for (size_t index = 0; index < size; index++) {
            if (block[index] != static_cast<uint8_t>(index)) {
                failures++;
                break;
            }
        }
        Heap::Free(block);
    }
    TEST_CHECK_EQ(failures, 0u);

    // Split blocks have to merge back, or the heap dies of fragmentation.
    TEST_CHECK_EQ(Heap::GetUsedBytes(), baseline);

    // Interleaved lifetimes, freed in the opposite order to catch a free list
    // that only merges the most recent split.
    void* blocks[32] = {};
    for (unsigned index = 0; index < 32; index++) {
        blocks[index] = Heap::Allocate(64 << (index % 5));
    }
    for (unsigned index = 32; index > 0; index--) {
        Heap::Free(blocks[index - 1]);
    }
    TEST_CHECK_EQ(Heap::GetUsedBytes(), baseline);

    // Reallocate moves the contents across.
    uint8_t* grown = static_cast<uint8_t*>(Heap::Allocate(32));
    TEST_CHECK(grown != nullptr);
    for (unsigned index = 0; index < 32; index++) {
        grown[index] = static_cast<uint8_t>(index + 1);
    }
    grown = static_cast<uint8_t*>(Heap::Reallocate(grown, 4096));
    TEST_CHECK(grown != nullptr);

    unsigned mismatches = 0;
    for (unsigned index = 0; index < 32; index++) {
        if (grown[index] != static_cast<uint8_t>(index + 1)) {
            mismatches++;
        }
    }
    TEST_CHECK_EQ(mismatches, 0u);
    Heap::Free(grown);
    TEST_CHECK_EQ(Heap::GetUsedBytes(), baseline);

    // A request larger than the heap fails instead of returning something.
    TEST_CHECK(Heap::Allocate(Heap::GetTotalBytes() * 2) == nullptr);
    TEST_CHECK_EQ(Heap::GetUsedBytes(), baseline);
}
