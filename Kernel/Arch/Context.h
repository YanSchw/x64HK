#pragma once
#include "Types.h"

// Everything a thread needs to have saved across a voluntary switch.
//
// Only the callee-saved registers appear here: ContextSwitch is reached through
// a normal call, so the compiler has already spilled whatever caller-saved
// registers it cared about. The offsets are hard-coded in Context.asm, hence
// the static_asserts below.
struct Context {
    uintptr_t Rbx;
    uintptr_t Rbp;
    uintptr_t R12;
    uintptr_t R13;
    uintptr_t R14;
    uintptr_t R15;
    void* Rsp;
} __attribute__((packed));

static_assert(__builtin_offsetof(Context, Rbx) == 0, "Context.asm expects Rbx at offset 0");
static_assert(__builtin_offsetof(Context, Rsp) == 48, "Context.asm expects Rsp at offset 48");
static_assert(sizeof(Context) == 56, "Context has the wrong size");

using KickoffFunction = void (*)(uintptr_t, uintptr_t, uintptr_t);

/// Builds a context that, on its first activation, enters InKickoff with the
/// three parameters. InStackTop is the high end of the thread's stack and must
/// be 16 byte aligned.
void PrepareContext(void* InStackTop, Context& OutContext, KickoffFunction InKickoff, uintptr_t InParam1,
                    uintptr_t InParam2, uintptr_t InParam3);

/// Saves the live registers into OutCurrent and loads InNext. Returns into the
/// new thread and, much later, back here when this thread is resumed.
extern "C" void ContextSwitch(Context* InNext, Context* OutCurrent);

/// Like ContextSwitch but throws the current context away. Used once per core to
/// leave the boot stack behind.
extern "C" [[noreturn]] void ContextLaunch(Context* InNext);

/// Trampoline that moves the kickoff parameters from the callee-saved registers
/// they were parked in into the argument registers the ABI expects.
extern "C" void FakeSystemVAbi();
