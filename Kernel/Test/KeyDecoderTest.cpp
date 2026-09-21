#include "Test/Test.h"
#include "Device/KeyDecoder.h"

// Scan code set 1 make codes used below. Break codes are the same byte with
// bit 7 set.
constexpr uint8_t MAKE_A = 0x1e;
constexpr uint8_t MAKE_B = 0x30;
constexpr uint8_t MAKE_LEFT_SHIFT = 0x2a;
constexpr uint8_t MAKE_LEFT_CTRL = 0x1d;
constexpr uint8_t MAKE_KP_8 = 0x48;  ///< Cursor up once prefixed with 0xe0
constexpr uint8_t BREAK_BIT = 0x80;
constexpr uint8_t PREFIX_EXTENDED = 0xe0;

void Test::RunKeyDecoderSuite() {
    Begin("KeyDecoder");

    // The decoder carries no global state, so each case can start clean.
    {
        KeyDecoder decoder;
        const Key pressed = decoder.Decode(MAKE_A);
        TEST_CHECK(pressed.IsValid());
        TEST_CHECK_EQ(pressed.Ascii(), 'a');
        TEST_CHECK(decoder.IsPressed(Key::ScanCode::A));

        // Only modifiers report their release; an ordinary key going up is not
        // a keystroke.
        const Key released = decoder.Decode(MAKE_A | BREAK_BIT);
        TEST_CHECK(!released.IsValid());
        TEST_CHECK(!decoder.IsPressed(Key::ScanCode::A));
    }

    {
        KeyDecoder decoder;
        const Key shiftDown = decoder.Decode(MAKE_LEFT_SHIFT);
        TEST_CHECK(!shiftDown.IsValid());

        const Key shifted = decoder.Decode(MAKE_A);
        TEST_CHECK(shifted.Shift);
        TEST_CHECK_EQ(shifted.Ascii(), 'A');

        decoder.Decode(MAKE_LEFT_SHIFT | BREAK_BIT);
        const Key plain = decoder.Decode(MAKE_A);
        TEST_CHECK(!plain.Shift);
        TEST_CHECK_EQ(plain.Ascii(), 'a');
    }

    {
        // The cursor block reuses the keypad's codes and is told apart only by
        // the prefix byte.
        KeyDecoder decoder;
        const Key prefix = decoder.Decode(PREFIX_EXTENDED);
        TEST_CHECK(!prefix.IsValid());

        const Key up = decoder.Decode(MAKE_KP_8);
        TEST_CHECK(up.IsValid());
        TEST_CHECK_EQ(ToUnderlying(up.Code), ToUnderlying(Key::ScanCode::UP));
        TEST_CHECK_EQ(static_cast<unsigned>(up.Ascii()), 0u);
    }

    {
        // A prefix applies to exactly one byte. If it leaked into the next one,
        // the left hand modifiers would report as the right hand ones forever.
        KeyDecoder decoder;
        decoder.Decode(PREFIX_EXTENDED);
        decoder.Decode(MAKE_LEFT_CTRL);

        const Key rightCtrl = decoder.Decode(MAKE_A);
        TEST_CHECK(rightCtrl.Ctrl());
        TEST_CHECK(rightCtrl.CtrlRight);
        TEST_CHECK(!rightCtrl.CtrlLeft);

        decoder.Decode(PREFIX_EXTENDED);
        decoder.Decode(MAKE_LEFT_CTRL | BREAK_BIT);
        TEST_CHECK(!decoder.Decode(MAKE_A).Ctrl());

        // Unprefixed this time, so it is the left hand key.
        decoder.Decode(MAKE_LEFT_CTRL);
        const Key leftCtrl = decoder.Decode(MAKE_B);
        TEST_CHECK(leftCtrl.CtrlLeft);
        TEST_CHECK(!leftCtrl.CtrlRight);
    }

    {
        // Bytes the decoder has no entry for must not corrupt what follows.
        KeyDecoder decoder;
        decoder.Decode(0x7f);
        const Key after = decoder.Decode(MAKE_A);
        TEST_CHECK(after.IsValid());
        TEST_CHECK_EQ(after.Ascii(), 'a');
    }
}
