#pragma once
#include "Types.h"

// The 8253/8254 Programmable Interval Timer. Its 1.193182 MHz input is fixed by
// history and independent of the CPU clock, which makes it the reference the
// LAPIC timer is calibrated against.
//
// Only channel 2 is used: it is the one the software can poll for expiry, and
// it also drives the PC speaker.
namespace Pit {

/// Arms channel 2. Caps out around 54.9 ms, the range of a 16-bit counter at
/// the base frequency. Returns false if the period does not fit.
bool Set(uint16_t InMicroseconds);

uint16_t Get();
bool IsActive();

/// Busy waits for the armed counter to expire.
bool WaitForTimeout();

bool Delay(uint16_t InMicroseconds);

/// Drives the PC speaker at InFrequency Hz, or silences it when given 0.
void PcSpeaker(uint32_t InFrequency);

void Disable();

}  // namespace Pit
