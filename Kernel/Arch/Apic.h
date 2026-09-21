#pragma once
#include "Types.h"

// System topology as read out of the ACPI MADT: which local APICs exist, where
// the I/O APIC is and how legacy IRQ lines map onto its pins.
namespace Apic {

/// Historic ISA interrupt line numbers, still how the I/O APIC pins are named.
enum class Device : uint8_t {
    TIMER = 0,
    KEYBOARD = 1,
    COM2 = 3,
    COM1 = 4,
    FLOPPY = 6,
    PRINTER = 7,
    REAL_TIME_CLOCK = 8,
    PS2_MOUSE = 12,
    IDE1 = 14,
    IDE2 = 15,
};

/// The xAPIC spec reserves the highest ID as the broadcast address.
constexpr uint8_t INVALID_ID = 0xff;

bool Initialize();

uintptr_t GetIoApicAddress();
uint8_t GetIoApicId();

/// Pin the device is wired to, after applying MADT interrupt source overrides.
uint8_t GetIoApicPin(Device InDevice);

/// Physical LAPIC ID of a core, or INVALID_ID.
uint8_t GetLocalApicId(unsigned InCoreId);

/// Logical LAPIC ID: exactly one bit set per core, so a bit mask can address a
/// group of cores in logical destination mode.
uint8_t GetLogicalApicId(unsigned InCoreId);

}  // namespace Apic
