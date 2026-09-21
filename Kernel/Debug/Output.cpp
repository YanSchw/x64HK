#include "Debug/Output.h"
#include "Arch/Cpu.h"
#include "Arch/IoPort.h"
#include "Config.h"

namespace Debug {

// Minimal COM1 writer with a constexpr constructor, so it is alive before any
// global constructor has run. It programs the UART on first use and then only
// polls the transmitter-holding-register-empty bit.
class EarlySerialStream : public OutputStream {
public:
    constexpr EarlySerialStream() = default;

    void Flush() override {
        if (!m_Initialized) {
            Configure();
            m_Initialized = true;
        }
        for (size_t i = 0; i < m_Position; i++) {
            if (m_Buffer[i] == '\n') {
                WriteByte('\r');
            }
            WriteByte(static_cast<uint8_t>(m_Buffer[i]));
        }
        m_Position = 0;
    }

private:
    static constexpr uint16_t PORT = 0x3f8;

    static void Configure() {
        IoPort(PORT + 1).OutB(0x00);  // no UART interrupts
        IoPort(PORT + 3).OutB(0x80);  // DLAB: the next two ports are the divisor
        IoPort(PORT + 0).OutB(0x01);  // divisor 1 -> 115200 baud
        IoPort(PORT + 1).OutB(0x00);
        IoPort(PORT + 3).OutB(0x03);  // 8 data bits, no parity, 1 stop bit
        IoPort(PORT + 2).OutB(0xc7);  // enable and clear the FIFOs
        IoPort(PORT + 4).OutB(0x03);  // DTR + RTS
    }

    static void WriteByte(uint8_t InByte) {
        // Bounded spin: on hardware without a UART the status bit never sets.
        for (int attempt = 0; attempt < 100'000; attempt++) {
            if ((IoPort(PORT + 5).InB() & 0x20) != 0) {
                break;
            }
        }
        IoPort(PORT).OutB(InByte);
    }

    bool m_Initialized = false;
};

constinit static EarlySerialStream s_EarlyStream;
constinit static OutputStream* s_Streams[Config::MAX_CORES] = {};

OutputStream& Out() {
    const unsigned core = Cpu::GetId();
    OutputStream* stream = core < Config::MAX_CORES ? s_Streams[core] : nullptr;
    return stream != nullptr ? *stream : s_EarlyStream;
}

void SetStream(unsigned InCoreId, OutputStream* InStream) {
    if (InCoreId < Config::MAX_CORES) {
        s_Streams[InCoreId] = InStream;
    }
}

}  // namespace Debug
