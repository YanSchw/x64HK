#pragma once
#include "Types.h"
#include "Config.h"
#include "Lib/Queue.h"
#include "Thread/Dispatcher.h"
#include "Thread/IdleThread.h"
#include "Thread/Thread.h"

// Round-robin scheduler over one global ready queue.
//
// The queue is intrusive and therefore unbounded: making a thread ready cannot
// fail, which matters because it happens from epilogues that have nowhere to
// report an error to. Every method here runs inside the Guard.
class Scheduler {
public:
    constexpr Scheduler() = default;

    Scheduler(const Scheduler&) = delete;
    Scheduler& operator=(const Scheduler&) = delete;

    /// Picks the first thread for the calling core and starts it. Called once
    /// per core; does not return.
    [[noreturn]] void Schedule();

    /// Adds a thread to the ready queue and pokes an idle core if there is one.
    void Ready(Thread* InThread);

    /// Takes the running thread out of circulation and switches away for good.
    void Exit();

    /// Marks a thread as dying. If it is running right now, an IPI makes that
    /// core notice; otherwise it is simply dropped from the ready queue.
    void Kill(Thread* InThread);

    /// Switches to the next ready thread. InRequeue false parks the caller,
    /// which is how a thread blocks on a semaphore or a timer.
    void Resume(bool InRequeue = true);

    Thread* Active() { return m_Dispatcher.Active(); }
    const Thread* Active() const { return m_Dispatcher.Active(); }

    bool IsActive(const Thread* InThread, unsigned InCoreId) {
        return m_Dispatcher.IsActive(InThread, InCoreId);
    }

    bool IsEmpty() const { return m_ReadyQueue.IsEmpty(); }

    /// True while the given core is running nothing but its idle thread.
    bool IsIdle(unsigned InCoreId);

private:
    /// Next runnable thread, or this core's idle thread when there is none.
    Thread* GetNext();

    Dispatcher m_Dispatcher;
    Queue<Thread, &Thread::m_QueueLink> m_ReadyQueue;
};
