#pragma once
#include "Types.h"

// A decoded keystroke: the scan code plus the modifier state at the time it was
// pressed.
struct Key {
    /// Scan code set 1 make codes, in numeric order. The names describe the
    /// physical key, not the character it produces.
    enum class ScanCode : uint8_t {
        INVALID = 0,
        ESCAPE,
        DIGIT_1,
        DIGIT_2,
        DIGIT_3,
        DIGIT_4,
        DIGIT_5,
        DIGIT_6,
        DIGIT_7,
        DIGIT_8,
        DIGIT_9,
        DIGIT_0,
        MINUS,
        EQUAL,
        BACKSPACE,
        TAB,
        Q,
        W,
        E,
        R,
        T,
        Y,
        U,
        I,
        O,
        P,
        OPEN_BRACKET,
        CLOSE_BRACKET,
        ENTER,
        LEFT_CTRL,
        A,
        S,
        D,
        F,
        G,
        H,
        J,
        K,
        L,
        SEMICOLON,
        APOSTROPHE,
        GRAVE,
        LEFT_SHIFT,
        BACKSLASH,
        Z,
        X,
        C,
        V,
        B,
        N,
        M,
        COMMA,
        PERIOD,
        SLASH,
        RIGHT_SHIFT,
        KP_STAR,
        LEFT_ALT,
        SPACE,
        CAPS_LOCK,
        F1,
        F2,
        F3,
        F4,
        F5,
        F6,
        F7,
        F8,
        F9,
        F10,
        NUM_LOCK,
        SCROLL_LOCK,
        KP_7,
        KP_8,
        KP_9,
        KP_MINUS,
        KP_4,
        KP_5,
        KP_6,
        KP_PLUS,
        KP_1,
        KP_2,
        KP_3,
        KP_0,
        KP_PERIOD,
        SYSREQ,
        EUROPE_2,
        F11,
        F12,
        KP_EQUAL,

        COUNT,

        // The cursor block and the numeric keypad share these make codes; they
        // are told apart by the 0xe0 prefix byte.
        DELETE = KP_PERIOD,
        UP = KP_8,
        DOWN = KP_2,
        LEFT = KP_4,
        RIGHT = KP_6,
    };

    ScanCode Code = ScanCode::INVALID;

    bool Shift : 1 = false;
    bool AltLeft : 1 = false;
    bool AltRight : 1 = false;
    bool CtrlLeft : 1 = false;
    bool CtrlRight : 1 = false;
    bool CapsLock : 1 = false;
    bool NumLock : 1 = false;
    bool ScrollLock : 1 = false;

    constexpr Key() = default;

    bool IsValid() const { return Code != ScanCode::INVALID && Code < ScanCode::COUNT; }
    void Invalidate() { Code = ScanCode::INVALID; }

    /// US layout character, or 0 for keys that do not produce one.
    unsigned char Ascii() const;

    bool Alt() const { return AltLeft || AltRight; }
    bool Ctrl() const { return CtrlLeft || CtrlRight; }
};
