#pragma once
#include "Types.h"
#include "Lib/Queue.h"
#include "Thread/Thread.h"

struct Vault;

// Time-triggered wake-ups, driven by the scheduler tick.
//
// The bells are kept sorted with delta-encoded counters: each bell stores the
// number of ticks *after* its predecessor rather than an absolute deadline. A
// tick then only has to decrement the head, which makes the common case -- no
// bell is due -- O(1) instead of a walk over every sleeper.
class Bellringer {
public:
    constexpr Bellringer() = default;

    Bellringer(const Bellringer&) = delete;
    Bellringer& operator=(const Bellringer&) = delete;

    /// Advances time by one tick and readies every thread whose bell came due.
    void Check(Vault& InVault);

    /// Blocks the calling thread for roughly InMilliseconds, rounded to the
    /// scheduler tick.
    void Sleep(Vault& InVault, unsigned InMilliseconds);

    bool HasPendingBells() const { return !m_Bells.IsEmpty(); }

private:
    struct Bell {
        Bell* m_QueueLink = nullptr;
        Thread* Owner = nullptr;
        size_t Ticks = 0;  ///< Ticks after the previous bell, not an absolute time
    };

    using BellQueue = Queue<Bell, &Bell::m_QueueLink>;

    BellQueue m_Bells;
};
