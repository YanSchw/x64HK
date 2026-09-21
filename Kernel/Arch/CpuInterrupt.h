#pragma once
#include "Types.h"

namespace Cpu {
namespace Interrupt {

/// Bit 9 of RFLAGS mirrors the interrupt enable state.
constexpr uintptr_t FLAG_ENABLE = 1UL << 9;

/// Vectors 0..31 are architecturally defined; some of them push an error code
/// onto the stack, which changes the handler signature.
enum class Vector : uint8_t {
    DIVIDE_ERROR = 0,
    DEBUG = 1,
    NON_MASKABLE_INTERRUPT = 2,
    BREAKPOINT = 3,
    OVERFLOW = 4,
    BOUND_RANGE_EXCEEDED = 5,
    INVALID_OPCODE = 6,
    DEVICE_NOT_AVAILABLE = 7,
    DOUBLE_FAULT = 8,  // error code
    INVALID_TSS = 10,  // error code
    SEGMENT_NOT_PRESENT = 11,  // error code
    STACK_SEGMENT_FAULT = 12,  // error code
    GENERAL_PROTECTION_FAULT = 13,  // error code
    PAGE_FAULT = 14,  // error code, faulting address in CR2
    FLOATING_POINT_EXCEPTION = 16,
    ALIGNMENT_CHECK = 17,  // error code
    MACHINE_CHECK = 18,
    SIMD_FP_EXCEPTION = 19,
    VIRTUALIZATION_EXCEPTION = 20,
    CONTROL_PROTECTION = 21,  // error code
    SECURITY_EXCEPTION = 30,  // error code

    // Everything from 32 upwards is ours to assign.
    TIMER = 0x20,      ///< LAPIC timer, drives preemption
    KEYBOARD = 0x21,   ///< PS/2 controller via the I/O APIC
    ASSASSIN = 0x22,   ///< IPI: check the kill flag of the running thread
    WAKEUP = 0x23,     ///< IPI: leave the idle thread, work became available

    PANIC = 0xf2,      ///< Parked here so a stray I/O APIC entry is loud
    SPURIOUS = 0xff,   ///< Raised by the LAPIC itself; must not be acknowledged
};

constexpr size_t VECTOR_COUNT = 256;

inline bool IsEnabled() {
    uintptr_t flags;
    asm volatile("pushfq\n\tpop %0" : "=r"(flags) : : "memory");
    return (flags & FLAG_ENABLE) != 0;
}

/// `sti` only takes effect after the following instruction, which is what makes
/// the `sti; hlt` pair in Idle() race free.
inline void Enable() {
    asm volatile("sti\n\tnop" : : : "memory");
}

/// Returns whether interrupts *were* enabled, for pairing with Restore().
inline bool Disable() {
    const bool wasEnabled = IsEnabled();
    asm volatile("cli" : : : "memory");
    return wasEnabled;
}

/// Re-enables only if InWasEnabled says so, which makes disable/restore nest.
inline void Restore(bool InWasEnabled) {
    if (InWasEnabled) {
        Enable();
    }
}

/// RAII wrapper around Disable()/Restore().
class Guard {
public:
    Guard() : m_WasEnabled(Disable()) {}
    ~Guard() { Restore(m_WasEnabled); }

    Guard(const Guard&) = delete;
    Guard& operator=(const Guard&) = delete;

private:
    bool m_WasEnabled;
};

}  // namespace Interrupt
}  // namespace Cpu
