#pragma once
#include "Types.h"
#include "Arch/Cpu.h"

// First-come-first-served lock. Each contender draws a ticket and waits for the
// counter to reach it, which makes the hand-off order FIFO -- unlike a plain
// spinlock, where the core with the best cache locality tends to win every race.
class TicketLock {
public:
    consteval TicketLock() = default;
    ~TicketLock() = default;

    TicketLock(const TicketLock&) = delete;
    TicketLock& operator=(const TicketLock&) = delete;

    void Lock() {
        const uint32_t ticket = __atomic_fetch_add(&m_NextTicket, 1, __ATOMIC_RELAXED);
        while (__atomic_load_n(&m_NowServing, __ATOMIC_ACQUIRE) != ticket) {
            Cpu::Pause();
        }
    }

    void Unlock() { __atomic_fetch_add(&m_NowServing, 1, __ATOMIC_RELEASE); }

    bool IsLocked() const {
        return __atomic_load_n(&m_NowServing, __ATOMIC_ACQUIRE) !=
               __atomic_load_n(&m_NextTicket, __ATOMIC_ACQUIRE);
    }

    class Scope {
    public:
        explicit Scope(TicketLock& InLock) : m_Lock(InLock) { m_Lock.Lock(); }
        ~Scope() { m_Lock.Unlock(); }

        Scope(const Scope&) = delete;
        Scope& operator=(const Scope&) = delete;

    private:
        TicketLock& m_Lock;
    };

private:
    uint32_t m_NowServing = 0;
    uint32_t m_NextTicket = 0;
};
