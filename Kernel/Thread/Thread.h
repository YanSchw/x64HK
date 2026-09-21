#pragma once
#include "Types.h"
#include "Config.h"
#include "Arch/Context.h"

// A thread: a stack, a saved register context and the code to run on it.
//
// Derive and override Action() to give a thread something to do.
class Thread {
public:
    Thread();
    virtual ~Thread() = default;

    // Copying a thread would duplicate a stack that contains absolute pointers
    // into itself, so it is not allowed.
    Thread(const Thread&) = delete;
    Thread(Thread&&) = delete;
    Thread& operator=(const Thread&) = delete;
    Thread& operator=(Thread&&) = delete;

    /// The thread body. The default implementation returns immediately, which
    /// is only valid because Kickoff catches that case.
    virtual void Action();

    /// Activates this thread on the calling core, abandoning the current stack.
    /// Used once per core to leave the boot stack.
    [[noreturn]] void Go();

    /// Saves this thread's registers and switches to InNext.
    void Resume(Thread* InNext);

    /// Marked by Scheduler::Kill; checked before the thread is scheduled again.
    bool IsDying() const { return __atomic_load_n(&m_Dying, __ATOMIC_ACQUIRE); }
    void MarkDying() { __atomic_store_n(&m_Dying, true, __ATOMIC_RELEASE); }

    /// False once the stack has grown past its low end and clobbered the canary.
    bool HasIntactStack() const;

    /// Link field for Lib/Queue. Public because a member pointer template
    /// argument has to be accessible at the point of use. A thread is only ever
    /// in one queue at a time -- ready, or waiting on one semaphore.
    Thread* m_QueueLink = nullptr;

private:
    /// First thing a new thread executes. It exists because a thread starts by
    /// returning into code rather than being called, so there has to be
    /// something that turns the raw entry into a virtual Action() call.
    [[noreturn]] static void Kickoff(uintptr_t InThread, uintptr_t InParam2, uintptr_t InParam3);

    Context m_Context;
    bool m_Dying = false;

    /// Interrupt frames and epilogues run on this stack too, not just the
    /// thread body, so it is considerably larger than the code alone needs.
    alignas(16) uint8_t m_Stack[Config::THREAD_STACK_SIZE];
};
