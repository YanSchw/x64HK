#pragma once
#include "Types.h"

// The legacy 8259A pair. We do not use it -- everything goes through the APICs
// -- but it has to be reprogrammed anyway, because after reset it raises its
// IRQs on vectors 8..15, which collide with the CPU exceptions.
namespace Pic {

/// Remaps both PICs out of the exception range and then masks every line.
void Initialize();

}  // namespace Pic
