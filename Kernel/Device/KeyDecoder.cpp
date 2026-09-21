#include "Device/KeyDecoder.h"
#include "Device/Ps2Controller.h"

/// Bit 7 set turns a make code into the matching break code.
constexpr uint8_t BREAK_BIT = 0x80;
constexpr uint8_t PREFIX_EXTENDED = 0xe0;
constexpr uint8_t PREFIX_PAUSE = 0xe1;

Key KeyDecoder::Decode(uint8_t InCode) {
    Key key = m_Modifiers;

    if (InCode == PREFIX_EXTENDED || InCode == PREFIX_PAUSE) {
        m_Prefix = InCode;
        return key;
    }

    const bool pressed = (InCode & BREAK_BIT) == 0;
    const Key::ScanCode code = static_cast<Key::ScanCode>(InCode & ~BREAK_BIT);

    if (code < Key::ScanCode::COUNT) {
        m_Pressed[ToUnderlying(code)] = pressed;

        // Modifiers are the only keys where releasing matters.
        bool isModifier = true;
        switch (code) {
            case Key::ScanCode::LEFT_SHIFT:
            case Key::ScanCode::RIGHT_SHIFT:
                m_Modifiers.Shift = pressed;
                break;

            case Key::ScanCode::LEFT_ALT:
                // The right hand modifiers reuse the left hand codes and are
                // only told apart by the prefix byte.
                if (m_Prefix == PREFIX_EXTENDED) {
                    m_Modifiers.AltRight = pressed;
                } else {
                    m_Modifiers.AltLeft = pressed;
                }
                break;

            case Key::ScanCode::LEFT_CTRL:
                if (m_Prefix == PREFIX_EXTENDED) {
                    m_Modifiers.CtrlRight = pressed;
                } else {
                    m_Modifiers.CtrlLeft = pressed;
                }
                break;

            default:
                isModifier = false;
                break;
        }

        if (pressed && !isModifier) {
            switch (code) {
                case Key::ScanCode::CAPS_LOCK:
                    m_Modifiers.CapsLock = !m_Modifiers.CapsLock;
                    Ps2Controller::SetLed(Ps2Controller::Led::CAPS_LOCK, m_Modifiers.CapsLock);
                    break;

                case Key::ScanCode::SCROLL_LOCK:
                    m_Modifiers.ScrollLock = !m_Modifiers.ScrollLock;
                    Ps2Controller::SetLed(Ps2Controller::Led::SCROLL_LOCK, m_Modifiers.ScrollLock);
                    break;

                case Key::ScanCode::NUM_LOCK:
                    // Pause reaches us as Ctrl+NumLock, because that is the
                    // combination the original AT keyboard used for it.
                    if (m_Modifiers.CtrlLeft) {
                        key.Code = code;
                    } else {
                        m_Modifiers.NumLock = !m_Modifiers.NumLock;
                        Ps2Controller::SetLed(Ps2Controller::Led::NUM_LOCK, m_Modifiers.NumLock);
                    }
                    break;

                case Key::ScanCode::SLASH:
                    key.Code = code;
                    // The keypad divide key shares this code; it always means
                    // '/' regardless of shift.
                    if (m_Prefix == PREFIX_EXTENDED) {
                        key.Shift = false;
                    }
                    break;

                default:
                    key.Code = code;
                    // The cursor block shares the keypad's codes. Pretending
                    // num lock is off for prefixed keys keeps the arrows
                    // working while the keypad still types digits.
                    if (m_Modifiers.NumLock && m_Prefix == PREFIX_EXTENDED) {
                        key.NumLock = false;
                    }
                    break;
            }
        }
    }

    // A prefix only applies to the byte right after it.
    m_Prefix = 0;
    return key;
}
