#pragma once
#include "Types.h"
#include "Arch/CpuInterrupt.h"
#include "Arch/IoApicRegisters.h"

// The I/O APIC routes external device interrupts to the local APICs. Its state
// is one redirection table indexed by pin number.
namespace IoApic {

/// Masks every pin and points them at the panic vector, so a device that fires
/// before it was configured is noticed rather than silently misdelivered.
void Initialize();

/// Routes a pin to an interrupt vector. Leaves the pin masked.
void Configure(uint8_t InPin, Cpu::Interrupt::Vector InVector, TriggerMode InTriggerMode = TriggerMode::EDGE,
               Polarity InPolarity = Polarity::HIGH);

void Allow(uint8_t InPin);
void Forbid(uint8_t InPin);
bool IsAllowed(uint8_t InPin);

}  // namespace IoApic
