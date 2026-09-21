#include "Arch/Pit.h"
#include "Arch/Cpu.h"
#include "Arch/IoPort.h"

namespace Pit {

constexpr uint8_t CHANNEL = 2;
constexpr uint64_t BASE_FREQUENCY = 1'193'182;

static const IoPort s_Data(0x40 + CHANNEL);
static const IoPort s_ModeRegister(0x43);
static const IoPort s_ControlRegister(0x61);

enum class AccessMode : uint8_t {
    LATCH_COUNT = 0,
    LOW_BYTE = 1,
    HIGH_BYTE = 2,
    LOW_AND_HIGH_BYTE = 3,
};

enum class OperatingMode : uint8_t {
    INTERRUPT_ON_TERMINAL_COUNT = 0,
    PROGRAMMABLE_ONE_SHOT = 1,
    RATE_GENERATOR = 2,
    SQUARE_WAVE_GENERATOR = 3,
    SOFTWARE_TRIGGERED_STROBE = 4,
    HARDWARE_TRIGGERED_STROBE = 5,
};

enum class Format : uint8_t {
    BINARY = 0,
    BCD = 1,
};

union Mode {
    struct {
        Format NumberFormat : 1;
        OperatingMode Operating : 3;
        AccessMode Access : 2;
        uint8_t Channel : 2;
    };
    uint8_t Value;

    Mode(AccessMode InAccess, OperatingMode InOperating, Format InFormat)
        : NumberFormat(InFormat), Operating(InOperating), Access(InAccess), Channel(CHANNEL) {}

    /// Default: an all-zero mode byte latches the counter for reading.
    Mode() : Value(0) { Channel = CHANNEL; }

    void Write() const { s_ModeRegister.OutB(Value); }
};

/// NMI Status and Control Register. Its meaning drifted over the decades; this
/// is the modern ICH layout, not the original IBM PC XT one.
union Control {
    struct {
        uint8_t EnableTimerCounter2 : 1;  ///< Gate for channel 2
        uint8_t EnableSpeakerData : 1;    ///< Route channel 2 output to the speaker
        uint8_t EnablePciSerr : 1;
        uint8_t EnableNmiIoChk : 1;
        const uint8_t RefreshCycleToggle : 1;
        const uint8_t StatusTimerCounter2 : 1;  ///< Set when the counter expired
        const uint8_t StatusIoChkNmiSource : 1;
        const uint8_t StatusSerrNmiSource : 1;
    };
    uint8_t Value;

    Control() : Value(s_ControlRegister.InB()) {}

    /// The four status bits must be written back as zero.
    void Write() const { s_ControlRegister.OutB(Value & 0x0f); }
};

bool Set(uint16_t InMicroseconds) {
    const uint64_t counter = BASE_FREQUENCY * InMicroseconds / 1'000'000;
    if (counter > 0xffff) {
        return false;
    }

    // Gate the counter on but keep the speaker quiet; expiry then shows up in
    // the status bit without making noise.
    Control control;
    control.EnableSpeakerData = 0;
    control.EnableTimerCounter2 = 1;
    control.Write();

    Mode(AccessMode::LOW_AND_HIGH_BYTE, OperatingMode::INTERRUPT_ON_TERMINAL_COUNT, Format::BINARY).Write();

    s_Data.OutB(counter & 0xff);
    s_Data.OutB((counter >> 8) & 0xff);
    return true;
}

uint16_t Get() {
    Mode().Write();
    uint16_t value = s_Data.InB();
    value |= static_cast<uint16_t>(s_Data.InB()) << 8;
    return value;
}

bool IsActive() {
    const Control control;
    return control.EnableTimerCounter2 == 1 && control.StatusTimerCounter2 == 0;
}

bool WaitForTimeout() {
    while (true) {
        const Control control;
        if (control.EnableTimerCounter2 == 0) {
            return false;
        }
        if (control.StatusTimerCounter2 == 1) {
            return true;
        }
        Cpu::Pause();
    }
}

bool Delay(uint16_t InMicroseconds) {
    return Set(InMicroseconds) && WaitForTimeout();
}

void PcSpeaker(uint32_t InFrequency) {
    if (InFrequency == 0) {
        Disable();
        return;
    }

    Control control;
    uint64_t divisor = BASE_FREQUENCY / InFrequency;
    if (divisor > 0xffff) {
        divisor = 0xffff;
    }

    const bool alreadyPlaying = control.EnableSpeakerData != 0;
    if (!alreadyPlaying) {
        Mode(AccessMode::LOW_AND_HIGH_BYTE, OperatingMode::SQUARE_WAVE_GENERATOR, Format::BINARY).Write();
    }

    s_Data.OutB(divisor & 0xff);
    s_Data.OutB((divisor >> 8) & 0xff);

    // Enabling the speaker only after the divisor is loaded avoids a short burst
    // at whatever frequency was left over.
    if (!alreadyPlaying) {
        control.EnableSpeakerData = 1;
        control.EnableTimerCounter2 = 1;
        control.Write();
    }
}

void Disable() {
    Control control;
    control.EnableSpeakerData = 0;
    control.EnableTimerCounter2 = 0;
    control.Write();
}

}  // namespace Pit
