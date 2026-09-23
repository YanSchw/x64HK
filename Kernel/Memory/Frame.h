#pragma once
#include "Types.h"

// Physical page frame allocator over the boot loader's memory map.
//
// Only memory the loader reported as available is ever handed out, and only up
// to the reachable limit below, which starts at what the boot map covers and
// rises once Paging has mapped the rest.
namespace Frame {

constexpr size_t SIZE = 4 * KIB;

void Initialize();

/// Physical address of InCount contiguous frames, or 0 when none are left.
uintptr_t Allocate(size_t InCount = 1);

/// InAddress and InCount must match an earlier Allocate.
void Free(uintptr_t InAddress, size_t InCount = 1);

/// Frames past InLimit stay managed but are never handed out, because nothing
/// maps them. Paging raises this once its own tables cover them.
void SetReachableLimit(uintptr_t InLimit);

/// Frames backed by usable memory, whether currently taken or not.
size_t GetTotalFrames();
size_t GetFreeFrames();
size_t GetUsedFrames();

void Dump();

}  // namespace Frame
