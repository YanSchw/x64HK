#include "Thread/Thread.h"
#include "Interrupt/Guard.h"
#include "Debug/Assert.h"
#include "Debug/Panic.h"

Thread::Thread() {
    // Canary at the low end of the stack: the scheduler checks it on every
    // switch, which turns a silent overflow into a clear failure.
    *reinterpret_cast<uint64_t*>(m_Stack) = Config::STACK_CANARY;

    PrepareContext(m_Stack + Config::THREAD_STACK_SIZE, m_Context, Kickoff,
                   reinterpret_cast<uintptr_t>(this), 0, 0);
}

bool Thread::HasIntactStack() const {
    return *reinterpret_cast<const uint64_t*>(m_Stack) == Config::STACK_CANARY;
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
