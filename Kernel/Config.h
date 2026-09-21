#pragma once
#include "Types.h"

// Central tuning knobs. Anything that a second file would otherwise have to
// guess about lives here.
namespace Config {

/// Upper bound on CPU cores. Sizes every PerCore<> array, so keep it tight.
constexpr unsigned MAX_CORES = 8;

/// Stack handed to each core by the boot code, before threading exists.
constexpr size_t BOOT_STACK_SIZE = 16 * KIB;

/// Stack the IST switches to for faults that cannot trust the current one.
constexpr size_t IST_STACK_SIZE = 8 * KIB;

/// Per-thread stack. Interrupt frames and epilogues run on it too.
constexpr size_t THREAD_STACK_SIZE = 16 * KIB;

/// Period of the LAPIC timer that drives preemption and the Bellringer.
constexpr unsigned SCHEDULER_TICK_MS = 10;

/// Size of the kernel heap, as a power of two.
constexpr size_t HEAP_LOG2 = 24;

/// Where the real mode AP trampoline is relocated to. Must be below 1 MiB and
/// 4 KiB aligned, because a Startup-IPI only carries a page number.
constexpr uintptr_t AP_TRAMPOLINE_ADDRESS = 0x40000;

/// Pending epilogues per core.
constexpr unsigned EPILOGUE_QUEUE_SIZE = 64;

/// Decoded keystrokes buffered for the keyboard consumer.
constexpr unsigned KEY_BUFFER_SIZE = 32;

/// Written to the low end of every thread stack to detect overflow.
constexpr uint64_t STACK_CANARY = 0x7841'3634'484B'2121ULL;

}  // namespace Config
