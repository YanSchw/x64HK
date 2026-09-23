#include "Memory/KernelStack.h"
#include "Arch/CpuInterrupt.h"
#include "Config.h"
#include "Debug/Panic.h"
#include "Memory/Frame.h"
#include "Memory/Paging.h"
#include "Sync/SpinLock.h"

namespace KernelStack {

constexpr size_t GUARD_SIZE = Paging::PAGE_SIZE;
constexpr size_t STRIDE = Config::THREAD_STACK_SIZE + GUARD_SIZE;

// Threads are never destroyed, so handing out the next slot is enough.
static uintptr_t s_NextSlot = Config::KERNEL_STACK_BASE;
static SpinLock s_Lock;

uintptr_t Allocate() {
    Cpu::Interrupt::Guard interruptGuard;
    SpinLock::Scope lockGuard(s_Lock);

    const uintptr_t slot = s_NextSlot;
    s_NextSlot += STRIDE;

    const uintptr_t frames = Frame::Allocate(Config::THREAD_STACK_SIZE / Paging::PAGE_SIZE);
    if (frames == 0) {
        PANIC("Out of memory for a kernel stack");
    }

    if (!Paging::Map(slot + GUARD_SIZE, frames, Config::THREAD_STACK_SIZE,
                     Paging::Access::READ_WRITE)) {
        PANIC("Out of memory for a kernel stack mapping");
    }

    return slot + STRIDE;
}

bool IsGuardPage(uintptr_t InAddress) {
    if (InAddress < Config::KERNEL_STACK_BASE || InAddress >= s_NextSlot) {
        return false;
    }
    return (InAddress - Config::KERNEL_STACK_BASE) % STRIDE < GUARD_SIZE;
}

}  // namespace KernelStack
