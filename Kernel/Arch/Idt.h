#pragma once
#include "Types.h"
#include "Arch/CpuInterrupt.h"
#include "Lib/OutputStream.h"

// What the CPU pushes before entering a handler. Nothing else is saved
// automatically -- general purpose registers are the handler's problem, which
// is why every handler here carries the `interrupt` attribute and lets the
// compiler deal with it (and return via iretq).
struct InterruptContext {
    uintptr_t Ip;
    uintptr_t Cs : 16;
    uintptr_t : 0;  ///< The CPU pushes a full 64-bit slot for the 16-bit selector
    uintptr_t Flags;
    uintptr_t Sp;
    uintptr_t Ss : 16;
    uintptr_t : 0;
} __attribute__((packed));
static_assert(sizeof(InterruptContext) == 5 * 8, "InterruptContext has the wrong size");

/// Decoded #PF error code. The faulting linear address is in CR2.
struct PageFaultError {
    uint64_t Present : 1;    ///< 0 = page not present, 1 = protection violation
    uint64_t Write : 1;
    uint64_t User : 1;
    uint64_t ReservedWrite : 1;
    uint64_t InstructionFetch : 1;
    uint64_t : 59;

    explicit PageFaultError(uint64_t InError) { *reinterpret_cast<uint64_t*>(this) = InError; }

    friend OutputStream& operator<<(OutputStream& InOut, const PageFaultError& InError) {
        return InOut << "{ present: " << InError.Present << ", write: " << InError.Write
                     << ", user: " << InError.User << ", reserved: " << InError.ReservedWrite
                     << ", ifetch: " << InError.InstructionFetch << " }";
    }
} __attribute__((packed));

namespace Idt {

enum class GateType : uint8_t {
    INTERRUPT = 0x6,  ///< Clears IF on entry
    TRAP = 0x7,       ///< Leaves IF alone
};

enum class PrivilegeLevel : uint8_t {
    KERNEL = 0,
    USER = 3,
};

using ReturningHandler = void (*)(InterruptContext*);
using ReturningHandlerWithError = void (*)(InterruptContext*, uint64_t);
using DivergingHandler = void (*)(InterruptContext*);
using DivergingHandlerWithError = void (*)(InterruptContext*, uint64_t);

struct alignas(8) InterruptDescriptor {
    uint16_t AddressLow;
    uint16_t Selector;
    uint8_t Ist : 3;  ///< 0 keeps the current stack, 1..7 selects a TSS IST entry
    uint8_t : 5;
    GateType Type : 3;
    uint8_t Size : 1;  ///< Always 1 in long mode
    uint8_t : 1;
    PrivilegeLevel Dpl : 2;
    uint8_t Present : 1;
    uint64_t AddressHigh : 48;
    uint64_t : 0;

    InterruptDescriptor() = default;

    InterruptDescriptor(uintptr_t InHandler, uint8_t InIst, PrivilegeLevel InDpl)
        : AddressLow(InHandler & 0xffff),
          Selector(0x08),
          Ist(InIst),
          Type(GateType::INTERRUPT),
          Size(1),
          Dpl(InDpl),
          Present(1),
          AddressHigh((InHandler >> 16) & 0xffff'ffff'ffff) {}

    static InterruptDescriptor Returning(ReturningHandler InHandler, uint8_t InIst = 0,
                                         PrivilegeLevel InDpl = PrivilegeLevel::KERNEL) {
        return {reinterpret_cast<uintptr_t>(InHandler), InIst, InDpl};
    }

    static InterruptDescriptor ReturningWithError(ReturningHandlerWithError InHandler, uint8_t InIst = 0,
                                                  PrivilegeLevel InDpl = PrivilegeLevel::KERNEL) {
        return {reinterpret_cast<uintptr_t>(InHandler), InIst, InDpl};
    }

    static InterruptDescriptor Diverging(DivergingHandler InHandler, uint8_t InIst = 0,
                                         PrivilegeLevel InDpl = PrivilegeLevel::KERNEL) {
        return {reinterpret_cast<uintptr_t>(InHandler), InIst, InDpl};
    }

    static InterruptDescriptor DivergingWithError(DivergingHandlerWithError InHandler, uint8_t InIst = 0,
                                                  PrivilegeLevel InDpl = PrivilegeLevel::KERNEL) {
        return {reinterpret_cast<uintptr_t>(InHandler), InIst, InDpl};
    }
} __attribute__((packed));
static_assert(sizeof(InterruptDescriptor) == 16, "Idt::InterruptDescriptor has the wrong size");

void Set(Cpu::Interrupt::Vector InVector, InterruptDescriptor InDescriptor);

/// The table itself is shared; every core has to run lidt once.
void Load();

}  // namespace Idt
