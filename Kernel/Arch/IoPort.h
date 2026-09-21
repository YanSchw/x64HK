#pragma once
#include "Types.h"

// x86 keeps a 64 KiB I/O address space separate from memory, reachable only
// through the in/out instructions.
class IoPort {
public:
    explicit constexpr IoPort(uint16_t InAddress) : m_Address(InAddress) {}

    void OutB(uint8_t InValue) const { asm volatile("outb %0, %1" : : "a"(InValue), "Nd"(m_Address)); }
    void OutW(uint16_t InValue) const { asm volatile("outw %0, %1" : : "a"(InValue), "Nd"(m_Address)); }
    void OutL(uint32_t InValue) const { asm volatile("outl %0, %1" : : "a"(InValue), "Nd"(m_Address)); }

    uint8_t InB() const {
        uint8_t value;
        asm volatile("inb %1, %0" : "=a"(value) : "Nd"(m_Address));
        return value;
    }

    uint16_t InW() const {
        uint16_t value;
        asm volatile("inw %1, %0" : "=a"(value) : "Nd"(m_Address));
        return value;
    }

    uint32_t InL() const {
        uint32_t value;
        asm volatile("inl %1, %0" : "=a"(value) : "Nd"(m_Address));
        return value;
    }

    /// Short delay by bouncing off the unused POST port; some legacy chips need
    /// a few bus cycles between consecutive writes.
    static void Wait() { asm volatile("outb %%al, $0x80" : : "a"(0)); }

private:
    uint16_t m_Address;
};
