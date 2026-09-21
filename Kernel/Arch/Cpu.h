#pragma once
#include "Types.h"
#include "Config.h"
#include "Arch/ControlRegister.h"
#include "Arch/CpuInterrupt.h"
#include "Arch/Msr.h"

// Per-core state and the handful of instructions that only make sense on the
// core executing them.
namespace Cpu {

/// Dense core index in [0, Count()), derived from the LAPIC ID via a lookup
/// table. Not the LAPIC ID itself, which is sparse and firmware assigned.
unsigned GetId();

/// Brings the calling core online: builds the LAPIC ID table (first caller
/// only), programs the LAPIC and loads this core's TSS.
void Initialize();

/// Marks the calling core offline again.
void Shutdown();

unsigned Count();
unsigned CountOnline();
bool IsOnline(unsigned InCoreId);

/// Tells the core it is spinning, which both saves power and avoids the
/// memory-order violation pipeline flush on leaving the loop.
inline void Pause() {
    asm volatile("pause" : : : "memory");
}

/// Sleeps until the next interrupt. `sti` takes effect one instruction late,
/// so an interrupt cannot slip in between the two and leave us halted forever.
inline void Idle() {
    asm volatile("sti\n\thlt" : : : "memory");
}

/// Halts this core for good; only a reset brings it back.
[[noreturn]] inline void Die() {
    while (true) {
        asm volatile("cli\n\thlt" : : : "memory");
    }
}

}  // namespace Cpu
