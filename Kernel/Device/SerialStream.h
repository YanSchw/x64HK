#pragma once
#include "Types.h"
#include "Arch/Serial.h"
#include "Lib/OutputStream.h"

// VT100 console over the serial line. Attach with `screen /dev/ttyUSB0 115200`,
// or let QEMU forward it to stdout.
class SerialStream : public OutputStream, public Serial {
public:
    enum class Attribute : uint8_t {
        RESET = 0,
        BRIGHT = 1,
        DIM = 2,
        ITALIC = 3,
        UNDERSCORE = 4,
        BLINK = 5,
        REVERSE = 7,
        HIDDEN = 8,
    };

    enum class Color : uint8_t {
        BLACK = 0,
        RED = 1,
        GREEN = 2,
        YELLOW = 3,
        BLUE = 4,
        MAGENTA = 5,
        CYAN = 6,
        WHITE = 7,
        DEFAULT = 9,
    };

    explicit SerialStream(ComPort InPort = ComPort::COM1, BaudRate InBaudRate = BaudRate::B115200)
        : Serial(InPort, InBaudRate) {}

    void Flush() override;

    void SetForeground(Color InColor);
    void SetBackground(Color InColor);
    void SetAttribute(Attribute InAttribute);
    void ResetAttribute(Attribute InAttribute);

    /// Clears the terminal and restores default colours.
    void Reset();

    /// One-based, as VT100 counts.
    void SetPosition(unsigned InColumn, unsigned InRow);

private:
    void WriteEscape(const char* InSequence);
    void WriteNumber(unsigned InValue);
};
