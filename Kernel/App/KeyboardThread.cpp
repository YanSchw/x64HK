#include "App/KeyboardThread.h"
#include "Arch/Cga.h"
#include "Interrupt/Guard.h"

static constexpr unsigned ECHO_ROW = 0;
static constexpr Cga::Attribute ECHO_ATTRIBUTE(Cga::Color::BLACK, Cga::Color::LIGHT_GREY);

static void ClearEchoRow(TextStream& InOutput) {
    InOutput.SetPosition(0u, ECHO_ROW);
    for (unsigned column = 0; column < Cga::COLUMNS; column++) {
        InOutput << ' ';
    }
    InOutput << Flush;
}

void KeyboardThread::Action() {
    while (true) {
        Guarded guard = Guard::Enter();
        Vault& vault = guard.Vault();

        // Returns once the keyboard epilogue has produced something.
        vault.KeysAvailable.P(vault);

        Key key;
        if (!vault.Keys.Consume(key)) {
            continue;
        }

        const unsigned char character = key.Ascii();
        if (character == 0) {
            continue;
        }

        vault.Output.SetAttribute(ECHO_ATTRIBUTE);

        if (character == '\n') {
            ClearEchoRow(vault.Output);
            m_Column = 0;
            continue;
        }

        if (character == '\b') {
            if (m_Column > 0) {
                m_Column--;
                vault.Output.SetPosition(m_Column, ECHO_ROW);
                vault.Output << ' ' << Flush;
            }
            continue;
        }

        if (m_Column == Cga::COLUMNS) {
            ClearEchoRow(vault.Output);
            m_Column = 0;
        }

        vault.Output.SetPosition(m_Column, ECHO_ROW);
        vault.Output << static_cast<char>(character) << Flush;
        m_Column++;
    }
}
