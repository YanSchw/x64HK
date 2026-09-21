#pragma once
#include "Types.h"

// C startup code. Normally the C library calls the global constructors before
// main; in a freestanding kernel that is our job.
namespace Csu {

/// Runs every global constructor the compiler registered.
void RunInitializers();

/// Runs the global destructors. Only meaningful on the way down.
void RunFinalizers();

}  // namespace Csu
