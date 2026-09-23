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

/// Where those stacks are mapped, each one behind an unmapped guard page.
constexpr uintptr_t KERNEL_STACK_BASE = 0xFFFF'C000'0000'0000;

/// Period of the LAPIC timer that drives preemption and the Bellringer.
constexpr unsigned SCHEDULER_TICK_MS = 10;

/// Size of the kernel heap, as a power of two.
constexpr size_t HEAP_LOG2 = 24;

/// Where the kernel is linked. Boot/Sections.ld and the physical aliases it
/// exports have to agree with this.
constexpr uintptr_t KERNEL_VMA = 0xFFFF'FFFF'8000'0000;

/// Where all of physical memory is mapped, so the kernel can reach a frame
/// without a mapping of its own. Boot/LongMode.asm already puts the boot map
/// here, which is why nothing has to be rebased once Paging takes over.
constexpr uintptr_t DIRECT_MAP_BASE = 0xFFFF'8000'0000'0000;

/// Boot/LongMode.asm identity maps this much with 2 MiB pages. Physical memory
/// above it cannot be reached until the kernel builds page tables of its own.
constexpr uint64_t IDENTITY_MAPPED_LIMIT = 4 * GIB;

/// Where the real mode AP trampoline is relocated to. Must be below 1 MiB and
/// 4 KiB aligned, because a Startup-IPI only carries a page number.
constexpr uintptr_t AP_TRAMPOLINE_ADDRESS = 0x40000;

/// Pending epilogues per core.
constexpr unsigned EPILOGUE_QUEUE_SIZE = 64;

/// Decoded keystrokes buffered for the keyboard consumer.
constexpr unsigned KEY_BUFFER_SIZE = 32;

}  // namespace Config
