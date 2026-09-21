#include "Device/Ps2Controller.h"
#include "Device/KeyDecoder.h"
#include "Arch/Apic.h"
#include "Arch/CpuInterrupt.h"
#include "Arch/IoApic.h"
#include "Arch/IoPort.h"
#include "Debug/Output.h"
#include "Lib/RingBuffer.h"
#include "Sync/SpinLock.h"

namespace Ps2Controller {

// Status (read) and command (write) share one port, as do the device output
// (read) and input (write) buffers on the other. The names are from the
// controller's point of view, so "output buffer" is what we read from.
static const IoPort s_ControlPort(0x64);
static const IoPort s_DataPort(0x60);

enum Status : uint8_t {
    HAS_OUTPUT = 1 << 0,     ///< Something is waiting to be read
    INPUT_PENDING = 1 << 1,  ///< Our last write has not been consumed yet
    SYSTEM_FLAG = 1 << 2,
    IS_COMMAND = 1 << 3,
    IS_MOUSE = 1 << 5,  ///< The pending byte came from the auxiliary device
    TIMEOUT_ERROR = 1 << 6,
    PARITY_ERROR = 1 << 7,
};

/// Commands for the controller itself, written to the command port.
enum ControllerCommand : uint8_t {
    READ_CONFIGURATION = 0x20,
    WRITE_CONFIGURATION = 0x60,
    DISABLE_MOUSE_PORT = 0xa7,
    ENABLE_MOUSE_PORT = 0xa8,
    DISABLE_KEYBOARD_PORT = 0xad,
    ENABLE_KEYBOARD_PORT = 0xae,
};

/// Bits of the controller configuration byte.
enum Configuration : uint8_t {
    KEYBOARD_INTERRUPT = 1 << 0,   ///< Raise IRQ1 when a scan code arrives
    MOUSE_INTERRUPT = 1 << 1,      ///< Raise IRQ12
    KEYBOARD_CLOCK_OFF = 1 << 4,   ///< Inverted: set means the port is disabled
    MOUSE_CLOCK_OFF = 1 << 5,
    TRANSLATE_TO_SET1 = 1 << 6,    ///< Convert scan code set 2 into set 1
};

/// Commands for the keyboard, written to the data port.
enum KeyboardCommand : uint8_t {
    SET_LED = 0xed,
    ECHO = 0xee,
    SET_REPEAT_RATE = 0xf3,
    ENABLE_SCANNING = 0xf4,
    DISABLE_SCANNING = 0xf5,
    SET_DEFAULT = 0xf6,
};

enum Reply : uint8_t {
    ACKNOWLEDGE = 0xfa,
    RESEND = 0xfe,
};

static KeyDecoder s_Decoder;
static uint8_t s_Leds = 0;

/// Raw scan code bytes handed from the prologue to the epilogue. Prologues can
/// run on several cores at once (the I/O APIC delivers to the least busy one),
/// hence the lock; interrupts are already disabled when it is taken.
static RingBuffer<uint8_t, 64> s_RawBytes;
static SpinLock s_RawLock;

/// Bounded spins: a machine without an 8042 never clears these bits, and
/// hanging the kernel on absent hardware is not acceptable.
constexpr int SPIN_LIMIT = 100'000;

static bool WaitWritable() {
    for (int attempt = 0; attempt < SPIN_LIMIT; attempt++) {
        if ((s_ControlPort.InB() & Status::INPUT_PENDING) == 0) {
            return true;
        }
    }
    return false;
}

static bool WaitReadable() {
    for (int attempt = 0; attempt < SPIN_LIMIT; attempt++) {
        if ((s_ControlPort.InB() & Status::HAS_OUTPUT) != 0) {
            return true;
        }
    }
    return false;
}

static void SendCommand(uint8_t InCommand) {
    if (WaitWritable()) {
        s_ControlPort.OutB(InCommand);
    }
}

static void SendData(uint8_t InValue) {
    if (WaitWritable()) {
        s_DataPort.OutB(InValue);
    }
}

static int ReadData() {
    return WaitReadable() ? s_DataPort.InB() : -1;
}

/// Sends a byte to the keyboard and consumes its acknowledgement, retrying once
/// if the keyboard asks for a resend.
static bool SendToKeyboard(uint8_t InByte) {
    for (int attempt = 0; attempt < 3; attempt++) {
        SendData(InByte);
        const int reply = ReadData();
        if (reply == Reply::ACKNOWLEDGE) {
            return true;
        }
        if (reply != Reply::RESEND) {
            return false;
        }
    }
    return false;
}

void Initialize() {
    // Configure with interrupts off and both ports quiet, so nothing can inject
    // a byte into the middle of a command/parameter pair.
    Cpu::Interrupt::Guard interruptGuard;

    SendCommand(ControllerCommand::DISABLE_KEYBOARD_PORT);
    SendCommand(ControllerCommand::DISABLE_MOUSE_PORT);
    DrainBuffer();

    // Do not inherit whatever the firmware left behind: IRQ1 on, translation to
    // scan code set 1 on, keyboard clock running, mouse port off entirely.
    SendCommand(ControllerCommand::READ_CONFIGURATION);
    int configuration = ReadData();
    if (configuration < 0) {
        DBG << "PS/2: no controller found" << EndLine;
        return;
    }
    configuration |= Configuration::KEYBOARD_INTERRUPT | Configuration::TRANSLATE_TO_SET1 |
                     Configuration::MOUSE_CLOCK_OFF;
    configuration &= ~(Configuration::KEYBOARD_CLOCK_OFF | Configuration::MOUSE_INTERRUPT);

    SendCommand(ControllerCommand::WRITE_CONFIGURATION);
    SendData(static_cast<uint8_t>(configuration));

    SendCommand(ControllerCommand::ENABLE_KEYBOARD_PORT);

    // A keyboard comes out of reset with scanning disabled and will not send a
    // single scan code until told otherwise.
    SendToKeyboard(KeyboardCommand::ENABLE_SCANNING);

    SetRepeatRate(Speed::CPS_30, Delay::MS_250);

    s_Leds = 0;
    SendToKeyboard(KeyboardCommand::SET_LED);
    SendToKeyboard(s_Leds);

    DrainBuffer();

    // Level triggered rather than edge: if an interrupt is ever missed, the line
    // stays asserted and the I/O APIC re-delivers it instead of the keyboard
    // going dead.
    const uint8_t pin = Apic::GetIoApicPin(Apic::Device::KEYBOARD);
    IoApic::Configure(pin, Cpu::Interrupt::Vector::KEYBOARD, IoApic::TriggerMode::LEVEL);
    IoApic::Allow(pin);

    DBG_VERBOSE << "PS/2 keyboard on I/O APIC pin " << static_cast<unsigned>(pin) << EndLine;
}

void DrainToQueue() {
    SpinLock::Scope lockGuard(s_RawLock);

    // Bounded, so a chattering controller cannot pin a core inside a prologue.
    for (int attempt = 0; attempt < 16; attempt++) {
        const uint8_t status = s_ControlPort.InB();
        if ((status & Status::HAS_OUTPUT) == 0) {
            return;
        }

        const uint8_t code = s_DataPort.InB();
        if ((status & Status::IS_MOUSE) == 0) {
            s_RawBytes.Produce(code);
        }
    }
}

bool Fetch(Key& OutKey) {
    while (true) {
        uint8_t code = 0;
        {
            Cpu::Interrupt::Guard interruptGuard;
            SpinLock::Scope lockGuard(s_RawLock);
            if (!s_RawBytes.Consume(code)) {
                return false;
            }
        }

        // Command acknowledgements share the byte stream with scan codes.
        if (code == Reply::ACKNOWLEDGE || code == Reply::RESEND) {
            continue;
        }

        OutKey = s_Decoder.Decode(code);
        return true;
    }
}

void SetLed(Led InLed, bool InOn) {
    if (InOn) {
        s_Leds |= ToUnderlying(InLed);
    } else {
        s_Leds &= ~ToUnderlying(InLed);
    }
    // Not waiting for the acknowledgement here: this runs from the keyboard
    // epilogue, and the reply arrives as just another byte that Fetch drops.
    SendData(KeyboardCommand::SET_LED);
    SendData(s_Leds);
}

void SetRepeatRate(Speed InSpeed, Delay InDelay) {
    // One parameter byte: bits 0-4 are the rate, bits 5-6 the delay.
    const uint8_t parameter = ToUnderlying(InSpeed) | static_cast<uint8_t>(ToUnderlying(InDelay) << 5);
    SendToKeyboard(KeyboardCommand::SET_REPEAT_RATE);
    SendToKeyboard(parameter);
}

void DrainBuffer() {
    for (int attempt = 0; attempt < 32; attempt++) {
        if ((s_ControlPort.InB() & Status::HAS_OUTPUT) == 0) {
            return;
        }
        s_DataPort.InB();
    }
}

}  // namespace Ps2Controller
