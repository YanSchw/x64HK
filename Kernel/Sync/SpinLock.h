#pragma once
#include "Types.h"
#include "Arch/Cpu.h"

// Test-and-set lock. Fine for short, uncontended critical sections; it has no
// fairness guarantee, so a core can be starved indefinitely under load. Use
// TicketLock where that matters.
class SpinLock {
public:
    consteval SpinLock() = default;
    ~SpinLock() = default;

    SpinLock(const SpinLock&) = delete;
    SpinLock& operator=(const SpinLock&) = delete;

    void Lock() {
        while (__atomic_test_and_set(&m_Locked, __ATOMIC_ACQUIRE)) {
            Cpu::Pause();
        }
    }

    bool TryLock() { return !__atomic_test_and_set(&m_Locked, __ATOMIC_ACQUIRE); }

    void Unlock() { __atomic_clear(&m_Locked, __ATOMIC_RELEASE); }

    class Scope {
    public:
        explicit Scope(SpinLock& InLock) : m_Lock(InLock) { m_Lock.Lock(); }
        ~Scope() { m_Lock.Unlock(); }

        Scope(const Scope&) = delete;
        Scope& operator=(const Scope&) = delete;

    private:
        SpinLock& m_Lock;
    };

private:
    uint8_t m_Locked = 0;
};
