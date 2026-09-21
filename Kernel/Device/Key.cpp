#include "Device/Key.h"

namespace {

struct AsciiEntry {
    unsigned char Normal;
    unsigned char Shifted;
};

/// US layout, indexed by scan code.
constexpr AsciiEntry ASCII_TABLE[ToUnderlying(Key::ScanCode::COUNT)] = {
    {0, 0},             // INVALID
    {27, 27},           // ESCAPE
    {'1', '!'},         // DIGIT_1
    {'2', '@'},         // DIGIT_2
    {'3', '#'},         // DIGIT_3
    {'4', '$'},         // DIGIT_4
    {'5', '%'},         // DIGIT_5
    {'6', '^'},         // DIGIT_6
    {'7', '&'},         // DIGIT_7
    {'8', '*'},         // DIGIT_8
    {'9', '('},         // DIGIT_9
    {'0', ')'},         // DIGIT_0
    {'-', '_'},         // MINUS
    {'=', '+'},         // EQUAL
    {'\b', '\b'},       // BACKSPACE
    {'\t', '\t'},       // TAB
    {'q', 'Q'},         // Q
    {'w', 'W'},         // W
    {'e', 'E'},         // E
    {'r', 'R'},         // R
    {'t', 'T'},         // T
    {'y', 'Y'},         // Y
    {'u', 'U'},         // U
    {'i', 'I'},         // I
    {'o', 'O'},         // O
    {'p', 'P'},         // P
    {'[', '{'},         // OPEN_BRACKET
    {']', '}'},         // CLOSE_BRACKET
    {'\n', '\n'},       // ENTER
    {0, 0},             // LEFT_CTRL
    {'a', 'A'},         // A
    {'s', 'S'},         // S
    {'d', 'D'},         // D
    {'f', 'F'},         // F
    {'g', 'G'},         // G
    {'h', 'H'},         // H
    {'j', 'J'},         // J
    {'k', 'K'},         // K
    {'l', 'L'},         // L
    {';', ':'},         // SEMICOLON
    {'\'', '"'},        // APOSTROPHE
    {'`', '~'},         // GRAVE
    {0, 0},             // LEFT_SHIFT
    {'\\', '|'},        // BACKSLASH
    {'z', 'Z'},         // Z
    {'x', 'X'},         // X
    {'c', 'C'},         // C
    {'v', 'V'},         // V
    {'b', 'B'},         // B
    {'n', 'N'},         // N
    {'m', 'M'},         // M
    {',', '<'},         // COMMA
    {'.', '>'},         // PERIOD
    {'/', '?'},         // SLASH
    {0, 0},             // RIGHT_SHIFT
    {'*', '*'},         // KP_STAR
    {0, 0},             // LEFT_ALT
    {' ', ' '},         // SPACE
    {0, 0},             // CAPS_LOCK
    {0, 0},             // F1
    {0, 0},             // F2
    {0, 0},             // F3
    {0, 0},             // F4
    {0, 0},             // F5
    {0, 0},             // F6
    {0, 0},             // F7
    {0, 0},             // F8
    {0, 0},             // F9
    {0, 0},             // F10
    {0, 0},             // NUM_LOCK
    {0, 0},             // SCROLL_LOCK
    {0, '7'},           // KP_7
    {0, '8'},           // KP_8
    {0, '9'},           // KP_9
    {'-', '-'},         // KP_MINUS
    {0, '4'},           // KP_4
    {0, '5'},           // KP_5
    {0, '6'},           // KP_6
    {'+', '+'},         // KP_PLUS
    {0, '1'},           // KP_1
    {0, '2'},           // KP_2
    {0, '3'},           // KP_3
    {0, '0'},           // KP_0
    {127, '.'},         // KP_PERIOD
    {0, 0},             // SYSREQ
    {'\\', '|'},        // EUROPE_2
    {0, 0},             // F11
    {0, 0},             // F12
    {'=', '='},         // KP_EQUAL
};

constexpr bool IsLetter(Key::ScanCode InCode) {
    return (InCode >= Key::ScanCode::Q && InCode <= Key::ScanCode::P) ||
           (InCode >= Key::ScanCode::A && InCode <= Key::ScanCode::L) ||
           (InCode >= Key::ScanCode::Z && InCode <= Key::ScanCode::M);
}

constexpr bool IsKeypadDigit(Key::ScanCode InCode) {
    return InCode >= Key::ScanCode::KP_7 && InCode <= Key::ScanCode::KP_PERIOD;
}

}  // namespace

unsigned char Key::Ascii() const {
    if (!IsValid()) {
        return '\0';
    }

    // The shifted column doubles as the "num lock on" column for the keypad,
    // which is why those entries have no unshifted character.
    const bool useShifted = Shift || (CapsLock && IsLetter(Code)) || (NumLock && IsKeypadDigit(Code));
    const AsciiEntry& entry = ASCII_TABLE[ToUnderlying(Code)];
    return useShifted ? entry.Shifted : entry.Normal;
}
