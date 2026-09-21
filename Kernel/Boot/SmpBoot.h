#pragma once
#include "Types.h"

// Starting the application processors.
namespace SmpBoot {

/// Copies the real mode trampoline below 1 MiB and sends the INIT/SIPI sequence
/// that wakes the other cores.
///
/// Must run with interrupts disabled: an interrupt in the middle of the
/// sequence can leave the LAPIC without the end-of-interrupt it is waiting for,
/// which wedges every later device interrupt.
void Boot();

}  // namespace SmpBoot
