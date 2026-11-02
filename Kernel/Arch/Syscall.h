#pragma once
#include "Types.h"

// The ring 3 entry point. ABI/SPECIFICATION.md is the contract; this is the
// kernel half of it.
namespace Syscall {

/// Per core, reached through KERNEL_GS_BASE. Arch/Syscall.asm hard-codes these
/// offsets, hence the static assertions next to it.
struct Area {
    uintptr_t KernelStack;  ///< The running thread's stack, as in TSS Rsp[0]
    uintptr_t UserStack;    ///< Scratch, live only inside the entry sequence
};

/// Programs EFER.SCE, STAR, LSTAR, SFMASK and KERNEL_GS_BASE. Once per core.
void Initialize(unsigned InCoreId);

/// Points this core's entry at a new stack. Follows the running thread, so the
/// dispatcher has to call it on every switch.
void SetKernelStack(unsigned InCoreId, uintptr_t InStackTop);

}  // namespace Syscall

/// Leaves the kernel for InEntry in ring 3. Reached once per user thread, from
/// the thread body; a system call returns with sysret instead.
extern "C" [[noreturn]] void EnterUserMode(uintptr_t InEntry, uintptr_t InStackTop);
