#pragma once
#include "Types.h"

namespace Interrupt {

/// Fills the IDT and loads it. Every vector gets a handler, so an unexpected
/// interrupt produces a readable message instead of a triple fault.
void InstallHandlers();

}  // namespace Interrupt
