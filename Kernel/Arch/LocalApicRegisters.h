#pragma once
#include "Types.h"

namespace LocalApic {

/// Physical address of the memory mapped register block. Identical on every
/// core -- each one sees its own registers behind it. Overridden from the MADT.
extern volatile uintptr_t s_BaseAddress;

using Register = uint32_t;

/// Offsets from s_BaseAddress. All registers are 32 bit and 16 byte aligned.
enum Index : uint16_t {
    IDENTIFICATION = 0x020,
    VERSION = 0x030,
    TASK_PRIORITY = 0x080,
    END_OF_INTERRUPT = 0x0b0,
    LOGICAL_DESTINATION = 0x0d0,
    DESTINATION_FORMAT = 0x0e0,
    SPURIOUS_INTERRUPT_VECTOR = 0x0f0,
    IN_SERVICE = 0x100,  ///< 8 registers, 16 bytes apart, one bit per vector
    INTERRUPT_COMMAND_LOW = 0x300,
    INTERRUPT_COMMAND_HIGH = 0x310,
    TIMER_CONTROL = 0x320,
    TIMER_INITIAL_COUNT = 0x380,
    TIMER_CURRENT_COUNT = 0x390,
    TIMER_DIVIDE_CONFIGURATION = 0x3e0,
};

inline Register Read(Index InIndex) {
    return *reinterpret_cast<volatile Register*>(s_BaseAddress + InIndex);
}

inline void Write(Index InIndex, Register InValue) {
    *reinterpret_cast<volatile Register*>(s_BaseAddress + InIndex) = InValue;
}

}  // namespace LocalApic
