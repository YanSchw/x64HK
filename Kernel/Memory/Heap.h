#pragma once
#include "Types.h"

// Kernel heap: a buddy allocator over one contiguous run of physical frames.
//
// Buddy allocation rounds every request up to a power of two and splits larger
// blocks in half until one fits. Freeing merges a block with its "buddy" (the
// neighbour it was split from, found by flipping one bit of the offset) as long
// as that one is free too, which keeps fragmentation bounded without a scan.
namespace Heap {

/// Takes its region from the frame allocator, so Frame::Initialize has to have
/// run first. Must precede any allocation.
void Initialize();

/// Returns 16 byte aligned memory, or nullptr when the request cannot be met.
void* Allocate(size_t InSize);

/// InPointer must have come from Allocate, or be nullptr.
void Free(void* InPointer);

/// Grows or shrinks an allocation, copying when it has to move.
void* Reallocate(void* InPointer, size_t InSize);

size_t GetTotalBytes();
size_t GetUsedBytes();

void Dump();

}  // namespace Heap

extern "C" {
void* malloc(size_t InSize);
void free(void* InPointer);
void* calloc(size_t InCount, size_t InSize);
void* realloc(void* InPointer, size_t InSize);
}
