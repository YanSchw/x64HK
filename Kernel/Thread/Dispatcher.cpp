#include "Thread/Dispatcher.h"
#include "Arch/Cpu.h"
#include "Arch/Gdt.h"
#include "Arch/Syscall.h"
#include "Debug/Assert.h"

/// Where this core re-enters the kernel from ring 3. Both paths need it and
/// both follow the running thread: an interrupt reads the TSS, a system call
/// reads the per core area.
static void SetEntryStack(const Thread* InThread) {
    const unsigned core = Cpu::GetId();
    Gdt::SetKernelStack(core, InThread->StackTop());
    Syscall::SetKernelStack(core, InThread->StackTop());
}

void Dispatcher::Go(Thread* InFirst) {
    ASSERT(InFirst != nullptr);
    m_ActiveThread.Set(InFirst);
    SetEntryStack(InFirst);
    InFirst->Go();
    __builtin_unreachable();
}

void Dispatcher::Dispatch(Thread* InNext) {
    Thread* current = Active();
    ASSERT(InNext != nullptr);
    ASSERT(current != nullptr);

    if (current == InNext) {
        return;
    }

    m_ActiveThread.Set(InNext);
    SetEntryStack(InNext);
    current->Resume(InNext);
}
