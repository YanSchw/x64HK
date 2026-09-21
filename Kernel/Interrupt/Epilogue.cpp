#include "Interrupt/Epilogue.h"
#include "Interrupt/Guard.h"
#include "Arch/Cpu.h"
#include "Arch/LocalApic.h"
#include "Arch/System.h"
#include "Device/Ps2Controller.h"
#include "Debug/Output.h"

void Epilogues::Keyboard(Vault& InVault) {
    // One interrupt can have buffered several bytes, and one keystroke can take
    // several bytes, so decode until the raw queue runs dry.
    Key key;
    while (Ps2Controller::Fetch(key)) {
        if (!key.IsValid()) {
            continue;
        }

        if (key.Ctrl() && key.Alt() && key.Code == Key::ScanCode::DELETE) {
            DBG << "Ctrl+Alt+Del" << EndLine;
            System::Reboot();
        }

        // Dropping the key when nobody is consuming them is better than
        // blocking an epilogue, which would stall every core.
        if (InVault.Keys.Produce(key)) {
            InVault.KeysAvailable.V(InVault);
        }
    }
}

void Epilogues::Timer(Vault& InVault) {
    // One core owns the clock, otherwise every tick would be counted N times.
    if (Cpu::GetId() == 0) {
        InVault.Bellringer.Check(InVault);
    }
    InVault.Scheduler.Resume();
}

void Epilogues::Assassin(Vault& InVault) {
    // Resume() drops the current thread if its dying flag is set.
    InVault.Scheduler.Resume();
}

void Epilogues::Wakeup(Vault& InVault) {
    // The idle thread masked the timer before halting; preemption is needed
    // again now that there is a real thread to run.
    LocalApic::Timer::SetMasked(false);
    InVault.Scheduler.Resume();
}
