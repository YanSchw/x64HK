#pragma once
#include "Types.h"

namespace System {

/// Resets the machine through the legacy System Control Port A.
[[noreturn]] void Reboot();

/// Asks the hypervisor to power off. Silently returns on real hardware, where
/// a proper shutdown needs ACPI tables we do not parse.
void Shutdown();

}  // namespace System
