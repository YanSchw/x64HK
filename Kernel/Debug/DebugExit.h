#pragma once
#include "Types.h"

// QEMU's isa-debug-exit device, the only way a guest can hand a status back to
// the host: writing V to its port makes QEMU exit with (V << 1) | 1. The device
// only exists when QEMU is started with it, so the write is compiled in for
// TEST builds only and is a no-op everywhere else.
namespace Debug {

enum class ExitCode : uint32_t {
    SUCCESS = 0x10,  ///< QEMU exits 33
    FAILURE = 0x11,  ///< QEMU exits 35
};

/// Returns when the device is absent, so every caller still has to stop itself.
void RequestExit(ExitCode InCode);

}  // namespace Debug
