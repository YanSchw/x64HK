#pragma once
#include "Types.h"

/// Image entry point (Boot/Entry.asm). The boot loader jumps here in 32-bit
/// protected mode.
extern "C" void StartupBsp();

/// First C++ code to run, on every core. Brings the core up far enough to call
/// Main or MainAp and never returns.
extern "C" [[noreturn]] void KernelInit();

/// Runs on the bootstrap processor only.
extern "C" int Main();

/// Runs on every core, including the bootstrap processor once Main returns.
extern "C" int MainAp();
