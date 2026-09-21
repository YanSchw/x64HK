#include "Lib/OutputStream.h"

OutputStream& OutputStream::operator<<(char InChar) {
    Put(InChar);
    return *this;
}

OutputStream& OutputStream::operator<<(unsigned char InChar) {
    return *this << static_cast<char>(InChar);
}

OutputStream& OutputStream::operator<<(const char* InString) {
    if (InString == nullptr) {
        return *this << "(null)";
    }
    while (*InString != '\0') {
        Put(*InString++);
    }
    return *this;
}

OutputStream& OutputStream::operator<<(bool InValue) {
    return *this << (InValue ? "true" : "false");
}

OutputStream& OutputStream::operator<<(short InValue) {
    return *this << static_cast<long long>(InValue);
}

OutputStream& OutputStream::operator<<(unsigned short InValue) {
    return *this << static_cast<unsigned long long>(InValue);
}

OutputStream& OutputStream::operator<<(int InValue) {
    return *this << static_cast<long long>(InValue);
}

OutputStream& OutputStream::operator<<(unsigned int InValue) {
    return *this << static_cast<unsigned long long>(InValue);
}

OutputStream& OutputStream::operator<<(long InValue) {
    return *this << static_cast<long long>(InValue);
}

OutputStream& OutputStream::operator<<(unsigned long InValue) {
    return *this << static_cast<unsigned long long>(InValue);
}

OutputStream& OutputStream::operator<<(long long InValue) {
    // Only decimal carries a sign; the other bases show the raw machine word,
    // so -1 prints as 0xffffffffffffffff.
    if (InValue < 0 && m_Base == 10) {
        Put('-');
        // Negating INT64_MIN overflows, so the magnitude is taken after the
        // cast to unsigned, where two's complement gives the right value.
        return *this << (~static_cast<unsigned long long>(InValue) + 1);
    }
    return *this << static_cast<unsigned long long>(InValue);
}

OutputStream& OutputStream::operator<<(unsigned long long InValue) {
    const unsigned long long base = static_cast<unsigned long long>(m_Base < 2 ? 16 : m_Base);

    if (base == 2) {
        *this << "0b";
    } else if (base == 8) {
        Put('0');
    } else if (base == 16) {
        *this << "0x";
    }

    unsigned long long divisor = 1;
    while (InValue / divisor >= base) {
        divisor *= base;
    }

    for (; divisor > 0; divisor /= base) {
        const unsigned long long digit = InValue / divisor;
        Put(digit < 10 ? static_cast<char>('0' + digit) : static_cast<char>('a' + digit - 10));
        InValue %= divisor;
    }
    return *this;
}

OutputStream& OutputStream::operator<<(const void* InPointer) {
    const int previousBase = m_Base;
    m_Base = 16;
    *this << reinterpret_cast<uintptr_t>(InPointer);
    m_Base = previousBase;
    return *this;
}

OutputStream& OutputStream::operator<<(OutputStream& (*InManipulator)(OutputStream&)) {
    return InManipulator(*this);
}

OutputStream& Flush(OutputStream& InOut) {
    InOut.Flush();
    return InOut;
}

OutputStream& EndLine(OutputStream& InOut) {
    InOut << '\n';
    InOut.Flush();
    return InOut;
}

OutputStream& Bin(OutputStream& InOut) {
    InOut.m_Base = 2;
    return InOut;
}

OutputStream& Oct(OutputStream& InOut) {
    InOut.m_Base = 8;
    return InOut;
}

OutputStream& Dec(OutputStream& InOut) {
    InOut.m_Base = 10;
    return InOut;
}

OutputStream& Hex(OutputStream& InOut) {
    InOut.m_Base = 16;
    return InOut;
}
