#pragma once
#include "Types.h"

namespace KernelStack {

/// Top of a freshly mapped stack. Panics rather than returning nothing.
uintptr_t Allocate();

/// True when a faulting address landed on a guard page.
bool IsGuardPage(uintptr_t InAddress);

}  // namespace KernelStack
