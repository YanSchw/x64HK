#pragma once
#include "Types.h"
#include "Lib/PerCore.h"
#include "Thread/Thread.h"

// Tracks which thread is running where and performs the actual switch.
//
// The "life pointer" is per core, because on a multi-core system several
// threads are running at the same time.
class Dispatcher {
public:
    constexpr Dispatcher() = default;

    Dispatcher(const Dispatcher&) = delete;
    Dispatcher& operator=(const Dispatcher&) = delete;

    Thread* Active() { return m_ActiveThread.Get(); }
    const Thread* Active() const { return m_ActiveThread.Get(); }
    Thread* Active(unsigned InCoreId) { return m_ActiveThread[InCoreId]; }

    bool IsActive(const Thread* InThread, unsigned InCoreId) { return m_ActiveThread[InCoreId] == InThread; }

    /// Starts the first thread on the calling core. Does not return.
    [[noreturn]] void Go(Thread* InFirst);

    /// Switches the calling core from the current thread to InNext.
    void Dispatch(Thread* InNext);

private:
    PerCore<Thread*> m_ActiveThread;
};
