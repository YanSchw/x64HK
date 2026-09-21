#include "Arch/Pic.h"
#include "Arch/IoPort.h"

namespace Pic {

void Initialize() {
    const IoPort primaryCommand(0x20);
    const IoPort primaryData(0x21);
    const IoPort secondaryCommand(0xa0);
    const IoPort secondaryData(0xa1);

    enum InitCommandWord1 : uint8_t {
        ICW4_NEEDED = 1 << 0,
        SINGLE_MODE = 1 << 1,
        ADDRESS_INTERVAL_HALF = 1 << 2,
        LEVEL_TRIGGERED = 1 << 3,
        ALWAYS_ONE = 1 << 4,
    };

    enum InitCommandWord4 : uint8_t {
        MODE_8086 = 1 << 0,
        AUTO_EOI = 1 << 1,
        BUFFER_PRIMARY = 1 << 2,
        BUFFERED_MODE = 1 << 3,
        SPECIAL_FULLY_NESTED = 1 << 4,
    };

    // Writing ICW1 to the command port starts a four word init sequence; the
    // remaining words go to the data port in a fixed order.
    const uint8_t icw1 = ICW4_NEEDED | ALWAYS_ONE;
    primaryCommand.OutB(icw1);
    secondaryCommand.OutB(icw1);

    // ICW2: vector base. 0x20 and 0x28 put the 16 IRQs just above the
    // architecturally reserved exception vectors.
    primaryData.OutB(0x20);
    secondaryData.OutB(0x28);

    // ICW3: how the two chips are cascaded. The primary gets a bit mask, the
    // secondary the pin number.
    constexpr uint8_t CASCADE_PIN = 2;
    primaryData.OutB(1 << CASCADE_PIN);
    secondaryData.OutB(CASCADE_PIN);

    // ICW4: 8086 mode, and acknowledge interrupts automatically since nothing
    // will ever service them.
    const uint8_t icw4 = MODE_8086 | AUTO_EOI;
    primaryData.OutB(icw4);
    secondaryData.OutB(icw4);

    // OCW1: mask everything.
    secondaryData.OutB(0xff);
    primaryData.OutB(0xff);
}

}  // namespace Pic
