#include "Types.h"
#include "Memory/Heap.h"
#include "Debug/Panic.h"

// The bits of the C++ runtime the compiler assumes exist.

void* operator new(size_t InSize) {
    return Heap::Allocate(InSize);
}

void* operator new[](size_t InSize) {
    return Heap::Allocate(InSize);
}

void* operator new(size_t InSize, void* InPlace) {
    return InPlace;
}

void* operator new[](size_t InSize, void* InPlace) {
    return InPlace;
}

void operator delete(void* InPointer) noexcept {
    Heap::Free(InPointer);
}

void operator delete[](void* InPointer) noexcept {
    Heap::Free(InPointer);
}

void operator delete(void* InPointer, size_t InSize) noexcept {
    Heap::Free(InPointer);
}

void operator delete[](void* InPointer, size_t InSize) noexcept {
    Heap::Free(InPointer);
}

/// Reached when a virtual call lands on a pure virtual slot, which means an
/// object was used during construction or after destruction.
extern "C" [[noreturn]] void __cxa_pure_virtual() {
    PANIC("Pure virtual function called");
}
