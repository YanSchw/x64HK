#include "Arch/Serial.h"
#include "Arch/IoPort.h"

Serial::Serial(ComPort InPort, BaudRate InBaudRate, DataBits InDataBits, StopBits InStopBits,
               Parity InParity)
    : m_Port(InPort) {
    WriteRegister(RegisterIndex::INTERRUPT_ENABLE, 0);

    // Setting DLAB turns the first two ports into the baud rate divisor latch;
    // it has to be cleared again before the data register is usable.
    WriteRegister(RegisterIndex::LINE_CONTROL, ToUnderlying(RegisterMask::DIVISOR_LATCH_ACCESS));
    WriteRegister(RegisterIndex::DIVISOR_LOW, ToUnderlying(InBaudRate) & 0xff);
    WriteRegister(RegisterIndex::DIVISOR_HIGH, (ToUnderlying(InBaudRate) >> 8) & 0xff);
    WriteRegister(RegisterIndex::LINE_CONTROL,
                  ToUnderlying(InDataBits) | ToUnderlying(InStopBits) | ToUnderlying(InParity));

    WriteRegister(RegisterIndex::FIFO_CONTROL, ToUnderlying(RegisterMask::ENABLE_FIFO) |
                                                   ToUnderlying(RegisterMask::CLEAR_RECEIVE_FIFO) |
                                                   ToUnderlying(RegisterMask::CLEAR_TRANSMIT_FIFO));

    WriteRegister(RegisterIndex::MODEM_CONTROL, ToUnderlying(RegisterMask::DATA_TERMINAL_READY) |
                                                    ToUnderlying(RegisterMask::REQUEST_TO_SEND) |
                                                    ToUnderlying(RegisterMask::OUT_2));
}

uint8_t Serial::ReadRegister(RegisterIndex InRegister) const {
    return IoPort(ToUnderlying(m_Port) + ToUnderlying(InRegister)).InB();
}

void Serial::WriteRegister(RegisterIndex InRegister, uint8_t InValue) const {
    IoPort(ToUnderlying(m_Port) + ToUnderlying(InRegister)).OutB(InValue);
}

int Serial::Write(uint8_t InByte) {
    // Bounded: on a machine with no UART behind this port the ready bit never
    // sets, and blocking the kernel on missing hardware is not acceptable.
    int attempts = 1'000'000;
    while ((ReadRegister(RegisterIndex::LINE_STATUS) & ToUnderlying(RegisterMask::TRANSMIT_HOLDING_EMPTY)) ==
           0) {
        if (--attempts <= 0) {
            return -1;
        }
    }

    WriteRegister(RegisterIndex::TRANSMIT_BUFFER, InByte);

    constexpr uint8_t ERRORS = ToUnderlying(RegisterMask::OVERRUN_ERROR) |
                               ToUnderlying(RegisterMask::PARITY_ERROR) |
                               ToUnderlying(RegisterMask::FRAMING_ERROR);
    return (ReadRegister(RegisterIndex::LINE_STATUS) & ERRORS) != 0 ? -1 : InByte;
}

int Serial::Read() {
    if ((ReadRegister(RegisterIndex::LINE_STATUS) & ToUnderlying(RegisterMask::DATA_READY)) == 0) {
        return -1;
    }
    return ReadRegister(RegisterIndex::RECEIVE_BUFFER);
}
