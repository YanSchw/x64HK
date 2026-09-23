#include "Thread/Thread.h"
#include "Interrupt/Guard.h"
#include "Memory/KernelStack.h"
#include "Debug/Assert.h"
#include "Debug/Panic.h"

void Thread::Prepare() {
    if (m_StackTop != 0) {
        return;
    }

    m_StackTop = KernelStack::Allocate();
    PrepareContext(reinterpret_cast<void*>(m_StackTop), m_Context, Kickoff,
                   reinterpret_cast<uintptr_t>(this), 0, 0);
}

void Thread::Kickoff(uintptr_t InThread, uintptr_t InParam2, uintptr_t InParam3) {
    // The switch into this thread happened inside the Guard's critical section,
    // so the first thing a new thread has to do is leave it.
    Guard::Leave();

    Thread* thread = reinterpret_cast<Thread*>(InThread);
    ASSERT(thread != nullptr);

    thread->Action();

    // Returning would pop whatever PrepareContext left below, so a thread that
    // finishes has to take itself out of circulation explicitly.
    Guard::Enter().Vault().Scheduler.Exit();
    PANIC("Scheduler returned into a finished thread");
}

void Thread::Resume(Thread* InNext) {
    ASSERT(InNext != nullptr);
    ContextSwitch(&InNext->m_Context, &m_Context);
}

void Thread::Go() {
    ContextLaunch(&m_Context);
}

void Thread::Action() {}
