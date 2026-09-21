#pragma once
#include "Types.h"

// 8250/16550 UART. Still present as a chipset function on most boards and the
// only output device that works before anything else is initialised.
class Serial {
public:
    enum class ComPort : uint16_t {
        COM1 = 0x3f8,
        COM2 = 0x2f8,
        COM3 = 0x3e8,
        COM4 = 0x2e8,
    };

    /// Values are the divisor of the fixed 115200 Hz reference clock.
    enum class BaudRate : uint16_t {
        B300 = 384,
        B600 = 192,
        B1200 = 96,
        B2400 = 48,
        B4800 = 24,
        B9600 = 12,
        B19200 = 6,
        B38400 = 3,
        B57600 = 2,
        B115200 = 1,
    };

    // The values below are pre-shifted into their line control register position.
    enum class DataBits : uint8_t {
        D5 = 0,
        D6 = 1,
        D7 = 2,
        D8 = 3,
    };

    enum class StopBits : uint8_t {
        S1 = 0,
        S2 = 4,
    };

    enum class Parity : uint8_t {
        NONE = 0,
        ODD = 8,
        EVEN = 24,
        MARK = 40,
        SPACE = 56,
    };

    explicit Serial(ComPort InPort = ComPort::COM1, BaudRate InBaudRate = BaudRate::B115200,
                    DataBits InDataBits = DataBits::D8, StopBits InStopBits = StopBits::S1,
                    Parity InParity = Parity::NONE);

    /// Returns the byte written, or -1 on timeout or a line error.
    int Write(uint8_t InByte);

    /// Returns the byte read, or -1 if the receive buffer is empty.
    int Read();

protected:
    const ComPort m_Port;

private:
    enum class RegisterIndex : uint8_t {
        RECEIVE_BUFFER = 0,
        TRANSMIT_BUFFER = 0,
        DIVISOR_LOW = 0,   ///< Only while DLAB is set
        INTERRUPT_ENABLE = 1,
        DIVISOR_HIGH = 1,  ///< Only while DLAB is set
        INTERRUPT_IDENT = 2,
        FIFO_CONTROL = 2,
        LINE_CONTROL = 3,
        MODEM_CONTROL = 4,
        LINE_STATUS = 5,
        MODEM_STATUS = 6,
    };

    enum class RegisterMask : uint8_t {
        // FIFO control
        ENABLE_FIFO = 1 << 0,
        CLEAR_RECEIVE_FIFO = 1 << 1,
        CLEAR_TRANSMIT_FIFO = 1 << 2,
        // Line control
        DIVISOR_LATCH_ACCESS = 1 << 7,  ///< DLAB, remaps ports 0 and 1
        // Modem control
        DATA_TERMINAL_READY = 1 << 0,
        REQUEST_TO_SEND = 1 << 1,
        OUT_2 = 1 << 3,  ///< Gates the UART interrupt onto the bus
        // Line status
        DATA_READY = 1 << 0,
        OVERRUN_ERROR = 1 << 1,
        PARITY_ERROR = 1 << 2,
        FRAMING_ERROR = 1 << 3,
        TRANSMIT_HOLDING_EMPTY = 1 << 5,
    };

    uint8_t ReadRegister(RegisterIndex InRegister) const;
    void WriteRegister(RegisterIndex InRegister, uint8_t InValue) const;
};
