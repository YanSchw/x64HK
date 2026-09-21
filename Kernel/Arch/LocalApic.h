#pragma once
#include "Types.h"

// The local APIC built into every core. It delivers interrupts to its own core
// and is the only way to send one to another core.
namespace LocalApic {

/// Programs this core's LAPIC and publishes its logical ID.
void Initialize(uint8_t InLogicalId);

/// Acknowledges the interrupt currently being serviced. Must be called before
/// returning from a handler, or the LAPIC blocks every equal or lower priority
/// interrupt forever. Never call it for the spurious vector.
void EndOfInterrupt();

uint8_t GetId();
uint8_t GetLogicalId();
uint8_t GetVersion();

/// Highest vector currently in service on this core, or 0 if none. Useful to
/// identify an interrupt from inside a shared handler.
uint16_t GetInServiceVector();

// Inter-Processor Interrupts: how one core pokes another.
namespace Ipi {

/// True once the previous IPI has been accepted by its target.
bool IsDelivered();

void Send(uint8_t InDestinationApicId, uint8_t InVector);

/// Sends to every core whose bit is set in the logical destination mask.
void SendGroup(uint8_t InLogicalDestination, uint8_t InVector);

void SendAll(uint8_t InVector);
void SendOthers(uint8_t InVector);

/// Resets the other cores into their wait-for-SIPI state.
void SendInit(bool InAssert = true);

/// Starts the other cores at physical address InVector * 4096, in real mode.
void SendStartup(uint8_t InVector);

}  // namespace Ipi

// Core local timer, clocked off the bus frequency.
namespace Timer {

/// Measures LAPIC timer ticks per millisecond against the PIT, which runs at a
/// fixed 1.193182 MHz regardless of CPU speed.
uint32_t MeasureTickRate();

void Set(uint32_t InCounter, uint8_t InDivide, uint8_t InVector, bool InPeriodic, bool InMasked = false);

/// Works out counter and divider for the requested period. Runs once; the bus
/// frequency is the same on every core. Returns false if the period does not
/// fit into the 32-bit counter even at the largest divider.
bool Setup(uint32_t InMicroseconds);

uint32_t Interval();

/// Starts the periodic timer on the calling core using the values from Setup().
void Activate();

void SetMasked(bool InMasked);

}  // namespace Timer
}  // namespace LocalApic
