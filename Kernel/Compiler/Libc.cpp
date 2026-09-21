#include "Compiler/Libc.h"

// Emitted by the compiler and collected by the linker script.
extern void (*__preinit_array_start[])();
extern void (*__preinit_array_end[])();
extern void (*__init_array_start[])();
extern void (*__init_array_end[])();
extern void (*__fini_array_start[])();
extern void (*__fini_array_end[])();

namespace Csu {

void RunInitializers() {
    for (void (**entry)() = __preinit_array_start; entry != __preinit_array_end; entry++) {
        (*entry)();
    }
    for (void (**entry)() = __init_array_start; entry != __init_array_end; entry++) {
        (*entry)();
    }
}

void RunFinalizers() {
    // Reverse order, mirroring construction.
    for (void (**entry)() = __fini_array_end; entry != __fini_array_start;) {
        (*--entry)();
    }
}

}  // namespace Csu

// The compiler emits calls to these for objects with destructors at namespace
// scope. Nothing in a kernel ever "exits", so registration is a no-op.
extern "C" int atexit(void (*InFunction)()) {
    return 0;
}

extern "C" int __cxa_atexit(void (*InFunction)(void*), void* InArgument, void* InDsoHandle) {
    return 0;
}

extern "C" void* __dso_handle = nullptr;
