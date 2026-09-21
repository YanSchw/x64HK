#include "Arch/Gdt.h"
#include "Arch/Cache.h"
#include "Debug/Assert.h"
#include "Lib/String.h"

namespace Gdt {

// 32-bit table used between the boot loader handover and the switch to long
// mode. Flat 0..4 GiB, because the boot code addresses physical memory directly.
alignas(16) constinit static SegmentDescriptor s_ProtectedMode[] = {
    SegmentDescriptor::Null(),
    SegmentDescriptor::Segment(0, UINT32_MAX, true, 0, Size::BIT32),
    SegmentDescriptor::Segment(0, UINT32_MAX, false, 0, Size::BIT32),
};

// Long mode table. Slots 3 and up hold one 16-byte TSS descriptor per core, so
// the array is declared as plain 8-byte entries and written through below.
alignas(16) constinit static SegmentDescriptor s_LongMode[3 + 2 * Config::MAX_CORES] = {
    SegmentDescriptor::Null(),
    SegmentDescriptor::Segment64(true, 0),
    SegmentDescriptor::Segment64(false, 0),
};

// Referenced by Boot/Entry.asm and Boot/LongMode.asm.
extern "C" constexpr Pointer GdtProtectedModePointer(s_ProtectedMode);
extern "C" constexpr Pointer GdtLongModePointer(s_LongMode);

static TaskStateSegment s_TaskState[Config::MAX_CORES];
alignas(16) static uint8_t s_FaultStack[Config::MAX_CORES][Config::IST_STACK_SIZE];

// A 64-bit system descriptor is twice as wide as a code/data one: the base was
// widened to 64 bits and spills into the following table slot.
static void WriteTssDescriptor(unsigned InCoreId, const TaskStateSegment& InTss) {
    const uintptr_t base = reinterpret_cast<uintptr_t>(&InTss);
    const uint32_t limit = sizeof(TaskStateSegment) - 1;

    // Type 0x9 = available 64-bit TSS.
    const uint64_t low = (static_cast<uint64_t>(limit) & 0xffff) | ((base & 0xffffff) << 16) |
                         (0x9ULL << 40) | (1ULL << 47) | ((static_cast<uint64_t>(limit) >> 16 & 0xf) << 48) |
                         (((base >> 24) & 0xff) << 56);
    const uint64_t high = base >> 32;

    SegmentDescriptor* slot = &s_LongMode[3 + 2 * InCoreId];
    slot[0].Value = low;
    slot[1].Value = high;
}

void LoadTaskRegister(unsigned InCoreId) {
    ASSERT(InCoreId < Config::MAX_CORES);

    TaskStateSegment& tss = s_TaskState[InCoreId];
    memset(&tss, 0, sizeof(tss));

    // The stack grows down, so IST1 points past the end of the reserved block.
    tss.Ist[IST_FAULT_STACK - 1] =
        reinterpret_cast<uintptr_t>(s_FaultStack[InCoreId]) + Config::IST_STACK_SIZE;

    // An I/O permission bitmap offset beyond the segment limit means "no port
    // access outside ring 0", which is what we want.
    tss.IoMapBase = sizeof(TaskStateSegment);

    WriteTssDescriptor(InCoreId, tss);

    const uint16_t selector = FIRST_TSS + static_cast<uint16_t>(InCoreId * 16);
    asm volatile("ltr %0" : : "r"(selector) : "memory");
}

}  // namespace Gdt
