#pragma once
#include "Types.h"

// Physical page frame allocator over the boot loader's memory map.
//
// Only memory the loader reported as available is ever handed out, and only
// below Config::IDENTITY_MAPPED_LIMIT, because nothing above that can be
// addressed until the kernel builds its own page tables.
namespace Frame {

constexpr size_t SIZE = 4 * KIB;

void Initialize();

/// Physical address of InCount contiguous frames, or 0 when none are left.
uintptr_t Allocate(size_t InCount = 1);

/// InAddress and InCount must match an earlier Allocate.
void Free(uintptr_t InAddress, size_t InCount = 1);

/// Frames backed by usable memory, whether currently taken or not.
size_t GetTotalFrames();
size_t GetFreeFrames();
size_t GetUsedFrames();

void Dump();

}  // namespace Frame
