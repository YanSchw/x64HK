#pragma once
#include "Types.h"
#include "Lib/Queue.h"
#include "Thread/Thread.h"

struct Vault;

// Counting semaphore.
//
// Waiters are parked in an intrusive queue, so blocking never allocates and
// there is no upper bound on the number of waiting threads. Both operations
// have to run at epilogue level, which is why they take the Vault -- that
// reference is the proof that the caller holds the Guard.
class Semaphore {
public:
    explicit Semaphore(unsigned InInitialCount = 0) : m_Counter(InInitialCount) {}
    ~Semaphore() = default;

    Semaphore(const Semaphore&) = delete;
    Semaphore& operator=(const Semaphore&) = delete;

    /// Acquire. Blocks the calling thread while the counter is zero.
    void P(Vault& InVault);

    /// Release. Wakes one waiter, or bumps the counter if nobody is waiting.
    void V(Vault& InVault);

    unsigned Count() const { return m_Counter; }

private:
    unsigned m_Counter;
    Queue<Thread, &Thread::m_QueueLink> m_Waiting;
};
