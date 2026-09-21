#pragma once
#include "Types.h"

// Fixed capacity circular buffer. One slot is always left empty so that the
// full and empty states stay distinguishable without a separate counter.
//
// Safe for a single producer and a single consumer. Every other use needs
// external mutual exclusion (Guard lock or disabled interrupts).
template <typename T, unsigned CAPACITY>
class RingBuffer {
    static_assert(CAPACITY > 1, "A RingBuffer needs at least two slots");

public:
    constexpr RingBuffer() = default;

    RingBuffer(const RingBuffer&) = delete;
    RingBuffer& operator=(const RingBuffer&) = delete;

    /// Returns false when the buffer is full; the value is then dropped.
    bool Produce(const T& InValue) {
        const unsigned nextWrite = (m_Write + 1) % CAPACITY;
        if (nextWrite == m_Read) {
            return false;
        }
        m_Data[m_Write] = InValue;
        __atomic_store_n(&m_Write, nextWrite, __ATOMIC_RELEASE);
        return true;
    }

    /// Returns false when empty; OutValue is left untouched in that case.
    bool Consume(T& OutValue) {
        if (IsEmpty()) {
            return false;
        }
        OutValue = m_Data[m_Read];
        __atomic_store_n(&m_Read, (m_Read + 1) % CAPACITY, __ATOMIC_RELEASE);
        return true;
    }

    bool IsEmpty() const { return __atomic_load_n(&m_Read, __ATOMIC_ACQUIRE) == __atomic_load_n(&m_Write, __ATOMIC_ACQUIRE); }

    unsigned Count() const {
        const unsigned read = __atomic_load_n(&m_Read, __ATOMIC_ACQUIRE);
        const unsigned write = __atomic_load_n(&m_Write, __ATOMIC_ACQUIRE);
        return write >= read ? write - read : CAPACITY - read + write;
    }

    static constexpr unsigned Capacity() { return CAPACITY - 1; }

private:
    T m_Data[CAPACITY] = {};
    unsigned m_Write = 0;
    unsigned m_Read = 0;
};
