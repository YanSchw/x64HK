#pragma once
#include "Types.h"
#include "Device/Key.h"

// Turns the byte stream from the keyboard into Key objects.
//
// Scan code set 1 sends a make code when a key goes down and the same code with
// bit 7 set when it comes back up. Keys added after the original AT keyboard
// reuse existing codes and are distinguished by a 0xe0 (or 0xe1) prefix byte.
class KeyDecoder {
public:
    constexpr KeyDecoder() = default;

    /// Feeds one byte in. The returned Key is invalid while the sequence is
    /// still incomplete, or when the byte only changed a modifier.
    Key Decode(uint8_t InCode);

    /// Current up/down state of every key.
    bool IsPressed(Key::ScanCode InCode) const {
        return InCode < Key::ScanCode::COUNT && m_Pressed[ToUnderlying(InCode)];
    }

private:
    uint8_t m_Prefix = 0;
    Key m_Modifiers;
    bool m_Pressed[ToUnderlying(Key::ScanCode::COUNT)] = {};
};
