#include "App/KeyboardThread.h"
#include "Interrupt/Guard.h"
#include "Debug/Output.h"

void KeyboardThread::Action() {
    while (true) {
        Guarded guard = Guard::Enter();
        Vault& vault = guard.Vault();

        // Returns once the keyboard epilogue has produced something.
        vault.KeysAvailable.P(vault);

        Key key;
        while (vault.Keys.Consume(key)) {
            const unsigned char character = key.Ascii();
            if (character != 0) {
                DBG << static_cast<char>(character) << Flush;
            }
        }
    }
}
