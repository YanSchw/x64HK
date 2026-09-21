#include "Arch/LocalApic.h"
#include "Arch/LocalApicRegisters.h"
#include "Arch/CpuInterrupt.h"
#include "Arch/Pit.h"
#include "Types.h"

namespace LocalApic {
namespace Timer {

enum class Mode : uint8_t {
    ONE_SHOT = 0,
    PERIODIC = 1,
    DEADLINE = 2,
};

union ControlRegister {
    struct {
        uint32_t Vector : 8;
        uint32_t : 4;
        uint32_t DeliveryStatus : 1;  ///< Read only
        uint32_t : 3;
        uint32_t Masked : 1;
        Mode TimerMode : 2;
        uint32_t : 13;
    };
    Register Value;
} __attribute__((packed));

constexpr Register INVALID_DIVIDE = 0xff;

/// The divide configuration register uses a scattered 3-bit encoding: bit 2 is
/// skipped, so the values are not simply log2(divider).
static Register EncodeDivide(uint8_t InDivide) {
    switch (InDivide) {
        case 1: return 0xb;
        case 2: return 0x0;
        case 4: return 0x1;
        case 8: return 0x2;
        case 16: return 0x3;
        case 32: return 0x8;
        case 64: return 0x9;
        case 128: return 0xa;
        default: return INVALID_DIVIDE;
    }
}

static uint32_t s_Counter = 0;
static uint8_t s_Divider = 1;
static uint32_t s_IntervalMicroseconds = 0;

uint32_t MeasureTickRate() {
    constexpr uint16_t SAMPLE_MICROSECONDS = 50'000;

    Pit::Set(SAMPLE_MICROSECONDS);
    // Free-running and masked: only the counter matters here, not interrupts.
    Set(UINT32_MAX, 1, ToUnderlying(Cpu::Interrupt::Vector::TIMER), false, true);

    const uint32_t start = Read(Index::TIMER_CURRENT_COUNT);
    Pit::WaitForTimeout();
    const uint32_t end = Read(Index::TIMER_CURRENT_COUNT);

    // The LAPIC timer counts down, hence start - end.
    return (start - end) / (SAMPLE_MICROSECONDS / 1000);
}

void Set(uint32_t InCounter, uint8_t InDivide, uint8_t InVector, bool InPeriodic, bool InMasked) {
    const Register divide = EncodeDivide(InDivide);
    if (divide == INVALID_DIVIDE) {
        return;
    }

    ControlRegister control{};
    control.Vector = InVector;
    control.Masked = InMasked ? 1 : 0;
    control.TimerMode = InPeriodic ? Mode::PERIODIC : Mode::ONE_SHOT;

    // Writing the initial count is what actually arms the timer, so it goes last.
    Write(Index::TIMER_DIVIDE_CONFIGURATION, divide);
    Write(Index::TIMER_CONTROL, control.Value);
    Write(Index::TIMER_INITIAL_COUNT, InCounter);
}

bool Setup(uint32_t InMicroseconds) {
    const uint64_t ticksPerMillisecond = MeasureTickRate();
    const uint64_t ticks = static_cast<uint64_t>(InMicroseconds) * ticksPerMillisecond / 1000;

    // Pick the smallest divider that keeps the count inside 32 bits, so the
    // timer stays as fine grained as possible.
    uint8_t divider = 1;
    uint64_t counter = ticks;
    while (counter > UINT32_MAX && divider < 128) {
        divider *= 2;
        counter = ticks / divider;
    }
    if (counter > UINT32_MAX || counter == 0) {
        return false;
    }

    s_IntervalMicroseconds = InMicroseconds;
    s_Divider = divider;
    s_Counter = static_cast<uint32_t>(counter);
    return true;
}

uint32_t Interval() {
    return s_IntervalMicroseconds;
}

void Activate() {
    Set(s_Counter, s_Divider, ToUnderlying(Cpu::Interrupt::Vector::TIMER), true, false);
}

void SetMasked(bool InMasked) {
    ControlRegister control{};
    control.Value = Read(Index::TIMER_CONTROL);
    control.Masked = InMasked ? 1 : 0;
    Write(Index::TIMER_CONTROL, control.Value);
}

}  // namespace Timer
}  // namespace LocalApic
