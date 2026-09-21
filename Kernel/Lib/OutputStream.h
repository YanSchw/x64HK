#pragma once
#include "Types.h"
#include "Lib/StringBuffer.h"

// Formatting front end, roughly the freestanding equivalent of std::ostream.
// Backends derive from it and only have to implement Flush().
//
// Manipulators are plain functions taking and returning an OutputStream&, so
// `out << Hex << value << EndLine` works. Inside a class that derives from
// OutputStream, write `::Flush` to reach the manipulator instead of the member.
class OutputStream : public StringBuffer {
public:
    constexpr OutputStream() = default;
    ~OutputStream() override = default;

    void Flush() override = 0;

    /// Radix used for integers: 2, 8, 10 or 16.
    int m_Base = 10;

    OutputStream& operator<<(char InChar);
    OutputStream& operator<<(unsigned char InChar);
    OutputStream& operator<<(const char* InString);
    OutputStream& operator<<(bool InValue);
    OutputStream& operator<<(short InValue);
    OutputStream& operator<<(unsigned short InValue);
    OutputStream& operator<<(int InValue);
    OutputStream& operator<<(unsigned int InValue);
    OutputStream& operator<<(long InValue);
    OutputStream& operator<<(unsigned long InValue);
    OutputStream& operator<<(long long InValue);
    OutputStream& operator<<(unsigned long long InValue);
    OutputStream& operator<<(const void* InPointer);
    OutputStream& operator<<(OutputStream& (*InManipulator)(OutputStream&));
};

OutputStream& Flush(OutputStream& InOut);
OutputStream& EndLine(OutputStream& InOut);
OutputStream& Bin(OutputStream& InOut);
OutputStream& Oct(OutputStream& InOut);
OutputStream& Dec(OutputStream& InOut);
OutputStream& Hex(OutputStream& InOut);
