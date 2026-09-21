#pragma once
#include "Types.h"
#include "Device/Key.h"

// Intel 8042 keyboard controller.
//
// Long since absorbed into the chipset, and USB keyboards are presented through
// it by the firmware's legacy emulation, so this is still the simplest way to
// read a key.
namespace Ps2Controller {

enum class Led : uint8_t {
    SCROLL_LOCK = 1 << 0,
    NUM_LOCK = 1 << 1,
    CAPS_LOCK = 1 << 2,
};

/// Repeat rate in characters per second, encoded as the hardware wants it.
enum class Speed : uint8_t {
    CPS_30 = 0x00,
    CPS_20 = 0x04,
    CPS_10 = 0x0c,
    CPS_2 = 0x1f,
};

/// Delay before the repeat kicks in.
enum class Delay : uint8_t {
    MS_250 = 0,
    MS_500 = 1,
    MS_750 = 2,
    MS_1000 = 3,
};

/// Resets the LEDs, sets a fast repeat rate and routes the keyboard IRQ through
/// the I/O APIC.
void Initialize();

/// Prologue half: drains the controller's output buffer into an internal raw
/// byte queue. This has to happen with the interrupt still being serviced,
/// because the I/O APIC entry is level triggered -- leaving the byte in the
/// controller keeps the line asserted and re-raises the interrupt immediately.
void DrainToQueue();

/// Epilogue half: decodes buffered bytes until one yields a Key, skipping the
/// command acknowledgements that share the stream. False means the queue ran
/// dry; OutKey can still be invalid when the byte only advanced a sequence.
bool Fetch(Key& OutKey);

#ifdef TEST
/// Test seam: pushes a byte into the raw queue exactly as the prologue would,
/// so the decode path can be exercised without anyone touching a keyboard.
void InjectForTest(uint8_t InCode);
#endif

void SetLed(Led InLed, bool InOn);
void SetRepeatRate(Speed InSpeed, Delay InDelay);

/// Discards anything already buffered, e.g. keys pressed during boot.
void DrainBuffer();

}  // namespace Ps2Controller
