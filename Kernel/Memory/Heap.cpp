#include "Memory/Heap.h"
#include "Config.h"
#include "Memory/Frame.h"
#include "Arch/CpuInterrupt.h"
#include "Debug/Assert.h"
#include "Debug/Output.h"
#include "Debug/Panic.h"
#include "Lib/String.h"
#include "Sync/SpinLock.h"

namespace Heap {

/// Smallest block, header included. 16 bytes of header plus 16 usable, which
/// also keeps every returned pointer 16 byte aligned.
constexpr size_t MIN_ORDER = 5;
constexpr size_t MAX_ORDER = Config::HEAP_LOG2;
constexpr size_t ORDER_COUNT = MAX_ORDER - MIN_ORDER + 1;
constexpr size_t HEAP_SIZE = size_t{1} << MAX_ORDER;

static_assert(MAX_ORDER > MIN_ORDER, "Config::HEAP_LOG2 is too small");

/// Guards against freeing a pointer the allocator never handed out.
constexpr uint32_t BLOCK_MAGIC = 0x48'4b'42'4c;  // "HKBL"

/// Prefixes every block, free or not. Keeping the order in the block itself is
/// what lets Free find the buddy without any side table.
struct BlockHeader {
    uint32_t Magic;
    uint8_t Order;
    uint8_t IsFree;
    uint16_t Padding;
    BlockHeader* NextFree;
};
static_assert(sizeof(BlockHeader) == 16, "BlockHeader must keep payloads 16 byte aligned");

static uint8_t* s_Region = nullptr;
static BlockHeader* s_FreeLists[ORDER_COUNT];
static SpinLock s_Lock;
static size_t s_UsedBytes = 0;
static bool s_Initialized = false;

static constexpr size_t IndexOf(uint8_t InOrder) {
    return InOrder - MIN_ORDER;
}

static BlockHeader* BlockAt(size_t InOffset) {
    return reinterpret_cast<BlockHeader*>(s_Region + InOffset);
}

static size_t OffsetOf(const BlockHeader* InBlock) {
    return reinterpret_cast<const uint8_t*>(InBlock) - s_Region;
}

/// Smallest order whose block is at least InBytes.
static uint8_t OrderFor(size_t InBytes) {
    uint8_t order = MIN_ORDER;
    while ((size_t{1} << order) < InBytes && order < MAX_ORDER) {
        order++;
    }
    return order;
}

static void PushFree(BlockHeader* InBlock, uint8_t InOrder) {
    InBlock->Magic = BLOCK_MAGIC;
    InBlock->Order = InOrder;
    InBlock->IsFree = 1;
    InBlock->NextFree = s_FreeLists[IndexOf(InOrder)];
    s_FreeLists[IndexOf(InOrder)] = InBlock;
}

/// Unlinks a specific block. The free lists are short enough in practice that
/// the linear walk beats carrying a second pointer in every header.
static bool RemoveFree(BlockHeader* InBlock, uint8_t InOrder) {
    BlockHeader** link = &s_FreeLists[IndexOf(InOrder)];
    while (*link != nullptr) {
        if (*link == InBlock) {
            *link = InBlock->NextFree;
            InBlock->NextFree = nullptr;
            InBlock->IsFree = 0;
            return true;
        }
        link = &(*link)->NextFree;
    }
    return false;
}

void Initialize() {
    if (s_Initialized) {
        return;
    }

    s_Region = reinterpret_cast<uint8_t*>(Frame::Allocate(HEAP_SIZE / Frame::SIZE));
    if (s_Region == nullptr) {
        PANIC("Not enough physical memory for the kernel heap");
    }

    for (size_t i = 0; i < ORDER_COUNT; i++) {
        s_FreeLists[i] = nullptr;
    }
    PushFree(BlockAt(0), MAX_ORDER);
    s_UsedBytes = 0;
    s_Initialized = true;
}

void* Allocate(size_t InSize) {
    if (InSize == 0) {
        return nullptr;
    }

    // Concurrent allocation is rare but real (two cores, or a thread preempted
    // mid-allocation), so interrupts stay off for the whole critical section.
    Cpu::Interrupt::Guard interruptGuard;
    SpinLock::Scope lockGuard(s_Lock);

    if (!s_Initialized) {
        Initialize();
    }

    const size_t needed = InSize + sizeof(BlockHeader);
    if (needed > HEAP_SIZE) {
        return nullptr;
    }
    const uint8_t wanted = OrderFor(needed);

    // Find the smallest free block that can hold the request.
    uint8_t order = wanted;
    while (order <= MAX_ORDER && s_FreeLists[IndexOf(order)] == nullptr) {
        order++;
    }
    if (order > MAX_ORDER) {
        return nullptr;
    }

    BlockHeader* block = s_FreeLists[IndexOf(order)];
    s_FreeLists[IndexOf(order)] = block->NextFree;

    // Split down to the requested order, putting each discarded upper half back
    // on the free list one order below.
    while (order > wanted) {
        order--;
        PushFree(BlockAt(OffsetOf(block) + (size_t{1} << order)), order);
    }

    block->Magic = BLOCK_MAGIC;
    block->Order = wanted;
    block->IsFree = 0;
    block->NextFree = nullptr;
    s_UsedBytes += size_t{1} << wanted;

    return reinterpret_cast<uint8_t*>(block) + sizeof(BlockHeader);
}

void Free(void* InPointer) {
    if (InPointer == nullptr) {
        return;
    }

    Cpu::Interrupt::Guard interruptGuard;
    SpinLock::Scope lockGuard(s_Lock);

    BlockHeader* block = reinterpret_cast<BlockHeader*>(static_cast<uint8_t*>(InPointer) -
                                                        sizeof(BlockHeader));
    ASSERT(block->Magic == BLOCK_MAGIC);
    ASSERT(block->IsFree == 0);
    if (block->Magic != BLOCK_MAGIC || block->IsFree != 0) {
        return;
    }

    uint8_t order = block->Order;
    s_UsedBytes -= size_t{1} << order;

    // Merge upwards. The buddy of a block is its address with bit `order`
    // flipped, which is exactly the half it was split from.
    size_t offset = OffsetOf(block);
    while (order < MAX_ORDER) {
        const size_t buddyOffset = offset ^ (size_t{1} << order);
        BlockHeader* buddy = BlockAt(buddyOffset);

        if (buddy->Magic != BLOCK_MAGIC || buddy->IsFree == 0 || buddy->Order != order) {
            break;
        }
        if (!RemoveFree(buddy, order)) {
            break;
        }

        offset = offset < buddyOffset ? offset : buddyOffset;
        order++;
    }

    PushFree(BlockAt(offset), order);
}

void* Reallocate(void* InPointer, size_t InSize) {
    if (InPointer == nullptr) {
        return Allocate(InSize);
    }
    if (InSize == 0) {
        Free(InPointer);
        return nullptr;
    }

    const BlockHeader* block =
        reinterpret_cast<BlockHeader*>(static_cast<uint8_t*>(InPointer) - sizeof(BlockHeader));
    ASSERT(block->Magic == BLOCK_MAGIC);

    const size_t oldPayload = (size_t{1} << block->Order) - sizeof(BlockHeader);
    if (InSize <= oldPayload && OrderFor(InSize + sizeof(BlockHeader)) == block->Order) {
        return InPointer;
    }

    void* replacement = Allocate(InSize);
    if (replacement != nullptr) {
        memcpy(replacement, InPointer, InSize < oldPayload ? InSize : oldPayload);
        Free(InPointer);
    }
    return replacement;
}

size_t GetTotalBytes() {
    return HEAP_SIZE;
}

size_t GetUsedBytes() {
    return s_UsedBytes;
}

void Dump() {
    DBG << "Heap: " << Dec << (s_UsedBytes / KIB) << " KiB used of " << (HEAP_SIZE / KIB) << " KiB"
        << EndLine;
    for (uint8_t order = MIN_ORDER; order <= MAX_ORDER; order++) {
        unsigned count = 0;
        for (const BlockHeader* block = s_FreeLists[IndexOf(order)]; block != nullptr;
             block = block->NextFree) {
            count++;
        }
        if (count > 0) {
            DBG << "  2^" << Dec << static_cast<unsigned>(order) << ": " << count << " free" << EndLine;
        }
    }
}

}  // namespace Heap

extern "C" void* malloc(size_t InSize) {
    return Heap::Allocate(InSize);
}

extern "C" void free(void* InPointer) {
    Heap::Free(InPointer);
}

extern "C" void* calloc(size_t InCount, size_t InSize) {
    const size_t total = InCount * InSize;
    // Reject the multiplication overflowing rather than handing back a short
    // buffer the caller will happily run off the end of.
    if (InCount != 0 && total / InCount != InSize) {
        return nullptr;
    }
    void* memory = Heap::Allocate(total);
    if (memory != nullptr) {
        memset(memory, 0, total);
    }
    return memory;
}

extern "C" void* realloc(void* InPointer, size_t InSize) {
    return Heap::Reallocate(InPointer, InSize);
}
