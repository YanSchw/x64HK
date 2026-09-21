#include "Test/Test.h"
#include "Device/Ps2Controller.h"

void Test::RunPs2ControllerSuite() {
    Begin("Ps2Controller");

    // Nothing is typing during a headless run, but the controller may still
    // have left its own replies in the queue during Initialize.
    Key key;
    while (Ps2Controller::Fetch(key)) {
    }

    // The regression this suite exists for: an acknowledgement sits in the same
    // byte stream as the scan codes, and Fetch used to report it as "queue
    // empty". That ended the epilogue's drain loop and stranded every byte
    // behind it until the next key press.
    Ps2Controller::InjectForTest(0x1e);  // 'a' down
    Ps2Controller::InjectForTest(0xfa);  // acknowledgement, mid stream
    Ps2Controller::InjectForTest(0x30);  // 'b' down
    Ps2Controller::InjectForTest(0x9e);  // 'a' up
    Ps2Controller::InjectForTest(0xb0);  // 'b' up

    unsigned decoded = 0;
    char letters[4] = {};
    unsigned letterCount = 0;

    while (Ps2Controller::Fetch(key)) {
        decoded++;
        if (key.IsValid() && key.Ascii() != 0 && letterCount < 3) {
            letters[letterCount++] = static_cast<char>(key.Ascii());
        }
    }

    // Five bytes in, one of them skipped rather than treated as the end.
    TEST_CHECK_EQ(decoded, 4u);
    TEST_CHECK_EQ(letterCount, 2u);
    TEST_CHECK_EQ(letters[0], 'a');
    TEST_CHECK_EQ(letters[1], 'b');

    // A queue holding nothing but acknowledgements drains to empty instead of
    // wedging on the first one.
    Ps2Controller::InjectForTest(0xfa);
    Ps2Controller::InjectForTest(0xfa);
    TEST_CHECK(!Ps2Controller::Fetch(key));

    // And the byte after them still comes out.
    Ps2Controller::InjectForTest(0xfe);  // resend request
    Ps2Controller::InjectForTest(0x1e);  // 'a' down
    TEST_CHECK(Ps2Controller::Fetch(key));
    TEST_CHECK_EQ(key.Ascii(), 'a');

    Ps2Controller::InjectForTest(0x9e);
    while (Ps2Controller::Fetch(key)) {
    }
}
