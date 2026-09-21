#include "Arch/Cmos.h"
#include "Arch/CpuInterrupt.h"
#include "Arch/IoPort.h"

namespace Cmos {

static constexpr IoPort s_Address(0x70);
static constexpr IoPort s_Data(0x71);

namespace Nmi {

/// Bit 7 of the index port masks the non-maskable interrupt.
static constexpr uint8_t MASK = 0x80;
static bool s_Disabled = false;

void Enable() {
    Cpu::Interrupt::Guard guard;
    s_Address.OutB(s_Address.InB() & ~MASK);
    s_Disabled = false;
}

void Disable() {
    Cpu::Interrupt::Guard guard;
    s_Address.OutB(s_Address.InB() | MASK);
    s_Disabled = true;
}

bool IsEnabled() {
    s_Disabled = (s_Address.InB() & MASK) != 0;
    return !s_Disabled;
}

}  // namespace Nmi

static void SelectRegister(Register InRegister) {
    uint8_t value = ToUnderlying(InRegister);
    if (Nmi::s_Disabled) {
        value |= Nmi::MASK;
    } else {
        value &= ~Nmi::MASK;
    }
    s_Address.OutB(value);
}

uint8_t Read(Register InRegister) {
    Cpu::Interrupt::Guard guard;
    SelectRegister(InRegister);
    return s_Data.InB();
}

void Write(Register InRegister, uint8_t InValue) {
    Cpu::Interrupt::Guard guard;
    SelectRegister(InRegister);
    s_Data.OutB(InValue);
}

}  // namespace Cmos
