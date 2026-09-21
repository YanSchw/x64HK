#include "Device/SerialStream.h"

void SerialStream::Flush() {
    for (size_t i = 0; i < m_Position; i++) {
        // Terminals expect CRLF; the kernel only ever emits LF.
        if (m_Buffer[i] == '\n') {
            Serial::Write('\r');
        }
        Serial::Write(static_cast<uint8_t>(m_Buffer[i]));
    }
    m_Position = 0;
}

void SerialStream::WriteEscape(const char* InSequence) {
    Serial::Write(0x1b);
    Serial::Write('[');
    for (const char* character = InSequence; *character != '\0'; character++) {
        Serial::Write(static_cast<uint8_t>(*character));
    }
}

void SerialStream::WriteNumber(unsigned InValue) {
    char digits[12];
    unsigned count = 0;
    do {
        digits[count++] = static_cast<char>('0' + InValue % 10);
        InValue /= 10;
    } while (InValue > 0 && count < sizeof(digits));

    while (count > 0) {
        Serial::Write(static_cast<uint8_t>(digits[--count]));
    }
}

void SerialStream::SetForeground(Color InColor) {
    Flush();
    const char sequence[] = {'3', static_cast<char>('0' + ToUnderlying(InColor)), 'm', '\0'};
    WriteEscape(sequence);
}

void SerialStream::SetBackground(Color InColor) {
    Flush();
    const char sequence[] = {'4', static_cast<char>('0' + ToUnderlying(InColor)), 'm', '\0'};
    WriteEscape(sequence);
}

void SerialStream::SetAttribute(Attribute InAttribute) {
    Flush();
    const char sequence[] = {static_cast<char>('0' + ToUnderlying(InAttribute)), 'm', '\0'};
    WriteEscape(sequence);
}

void SerialStream::ResetAttribute(Attribute InAttribute) {
    Flush();
    // 2x turns off what x turned on.
    const char sequence[] = {'2', static_cast<char>('0' + ToUnderlying(InAttribute)), 'm', '\0'};
    WriteEscape(sequence);
}

void SerialStream::Reset() {
    Flush();
    WriteEscape("2J");
    SetForeground(Color::DEFAULT);
    SetBackground(Color::DEFAULT);
    SetAttribute(Attribute::RESET);
    SetPosition(1, 1);
}

void SerialStream::SetPosition(unsigned InColumn, unsigned InRow) {
    Flush();
    Serial::Write(0x1b);
    Serial::Write('[');
    WriteNumber(InRow);
    Serial::Write(';');
    WriteNumber(InColumn);
    Serial::Write('H');
}
