#pragma once
#include "Types.h"
#include "Config.h"

// Global Descriptor Table.
//
// Long mode barely segments any more: the only thing the CPU still reads out of
// a 64-bit code segment is the L bit and the privilege level, base and limit are
// ignored. The table is still mandatory, and it is also where the per-core Task
// State Segments live -- those carry the Interrupt Stack Table, which is how a
// double fault gets a known-good stack even when RSP is garbage.
namespace Gdt {

/// Selector values, i.e. byte offsets into the long mode table.
enum Selector : uint16_t {
    NULL_SELECTOR = 0x00,
    KERNEL_CODE = 0x08,
    KERNEL_DATA = 0x10,
    FIRST_TSS = 0x18,  ///< Core n uses FIRST_TSS + n * 16
};

enum class Granularity : uint64_t {
    BYTES = 0,
    PAGES = 1,
};

enum class Size : uint64_t {
    BIT16 = 0,
    BIT32 = 2,  ///< D/B set, L clear
    BIT64 = 1,  ///< D/B clear, L set
};

enum class TypeFlags : uint64_t {
    DATA_RW = 0b0010,
    CODE_RX = 0b1010,
};

union SegmentDescriptor {
    struct {
        uint64_t LimitLow : 16;
        uint64_t BaseLow : 24;
        uint64_t Type : 4;
        uint64_t IsCodeOrData : 1;  ///< 0 marks a system segment such as a TSS
        uint64_t PrivilegeLevel : 2;
        uint64_t Present : 1;
        uint64_t LimitHigh : 4;
        uint64_t Available : 1;
        uint64_t Custom : 2;  ///< L and D/B, see Size
        Granularity Granularity : 1;
        uint64_t BaseHigh : 8;
    } __attribute__((packed));
    uint64_t Value;

    consteval static SegmentDescriptor Null() { return SegmentDescriptor{.Value = 0}; }

    consteval static SegmentDescriptor Segment(uintptr_t InBase, uint32_t InLimit, bool InCode,
                                               uint64_t InRing, Size InSize) {
        return SegmentDescriptor{
            .LimitLow = (InLimit >> (InLimit > 0xfffff ? 12 : 0)) & 0xffff,
            .BaseLow = InBase & 0xffffff,
            .Type = InCode ? ToUnderlying(TypeFlags::CODE_RX) : ToUnderlying(TypeFlags::DATA_RW),
            .IsCodeOrData = 1,
            .PrivilegeLevel = InRing,
            .Present = 1,
            .LimitHigh = (InLimit > 0xfffff ? (InLimit >> 28) : (InLimit >> 16)) & 0xf,
            .Available = 0,
            .Custom = ToUnderlying(InSize),
            .Granularity = InLimit > 0xfffff ? Granularity::PAGES : Granularity::BYTES,
            .BaseHigh = (InBase >> 24) & 0xff,
        };
    }

    consteval static SegmentDescriptor Segment64(bool InCode, uint64_t InRing) {
        return Segment(0, 0, InCode, InRing, Size::BIT64);
    }
} __attribute__((packed));
static_assert(sizeof(SegmentDescriptor) == 8, "Gdt::SegmentDescriptor has the wrong size");

/// What lgdt/lidt expect: limit is the offset of the last valid byte, and the
/// little endian layout makes the same struct work for 16, 32 and 64 bit.
struct Pointer {
    uint16_t Limit;
    void* Base;

    template <typename T, size_t COUNT>
    explicit constexpr Pointer(const T (&InDescriptors)[COUNT])
        : Limit(COUNT * sizeof(T) - 1), Base(const_cast<T*>(InDescriptors)) {}

    consteval Pointer(void* InBase, size_t InCount)
        : Limit(InCount * sizeof(SegmentDescriptor) - 1), Base(InBase) {}

    constexpr void Set(void* InBase, size_t InCount) {
        Limit = InCount * sizeof(SegmentDescriptor) - 1;
        Base = InBase;
    }
} __attribute__((packed));
static_assert(sizeof(Pointer) == 10, "Gdt::Pointer has the wrong size");

/// Task State Segment. In long mode it no longer describes a hardware task; it
/// only supplies the ring 0 stack pointers and the seven IST entries.
struct TaskStateSegment {
    uint32_t Reserved0;
    uint64_t Rsp[3];  ///< Stack to switch to on entry into ring 0, 1, 2
    uint64_t Reserved1;
    uint64_t Ist[7];  ///< IST1..IST7, selected per IDT entry
    uint64_t Reserved2;
    uint16_t Reserved3;
    uint16_t IoMapBase;
} __attribute__((packed));
static_assert(sizeof(TaskStateSegment) == 104, "Gdt::TaskStateSegment has the wrong size");

/// IST slot (1-based, as the IDT encodes it) used by the faults that cannot
/// trust the interrupted stack.
constexpr uint8_t IST_FAULT_STACK = 1;

/// Replaces the table Boot/Entry.asm set up. Must run before LoadTaskRegister.
void Load();

/// Fills in this core's TSS descriptor, points IST1 at its emergency stack and
/// loads the task register.
void LoadTaskRegister(unsigned InCoreId);

}  // namespace Gdt
