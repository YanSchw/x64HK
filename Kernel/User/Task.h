#pragma once
#include "Types.h"

// A ring 3 program.
//
// There is one address space for everybody: the kernel's tables carry the user
// image and stacks in the lower half with U/S set, and its own half without.
// That is enough to keep ring 3 out of the kernel, which is what M4 is about;
// keeping two user tasks out of each other needs a page table per task.
namespace User {

struct Task {
    const char* Name = "user";
    uintptr_t Entry = 0;
    uintptr_t StackTop = 0;
};

/// Maps an embedded ELF image, one mapping per loadable segment so the W^X
/// split survives, and gives the task a stack. False if the image is not
/// something this kernel can run.
bool Load(const char* InName, const void* InImage, size_t InSize, Task& OutTask);

/// Top of a fresh user stack, one guard page above the last one. Load() calls
/// this; a caller that maps its own code needs it directly.
uintptr_t AllocateStack();

/// How tasks have ended. Counted unconditionally, because "how many programs
/// died and what did the last one die of" is worth knowing outside a test too.
struct Termination {
    unsigned Count;
    int Status;
};

Termination LastTermination();

/// Thread entry: drops the calling thread to ring 3 and never comes back.
[[noreturn]] void Run(void* InTask);

/// Ends the task running on the calling core. Reached from the Exit system call
/// and from the fault handlers, and never returns.
[[noreturn]] void Terminate(const char* InReason, int InStatus);

/// Copies InLength bytes out of ring 3 memory, refusing anything the user could
/// not have read itself.
bool CopyIn(void* OutDestination, uintptr_t InSource, size_t InLength);

}  // namespace User
