#include "App/CounterThread.h"
#include "Arch/Cpu.h"
#include "Interrupt/Guard.h"

void CounterThread::Action() {
    while (true) {
        {
            Guarded guard = Guard::Enter();
            Vault& vault = guard.Vault();

            vault.Output.SetAttribute(m_Attribute);
            vault.Output.SetPosition(0u, m_Row);
            vault.Output << m_Name << ' ' << Dec << m_Counter++ << " on core " << Cpu::GetId() << "   "
                         << Flush;
        }

        Guarded guard = Guard::Enter();
        guard.Vault().Bellringer.Sleep(guard.Vault(), 1000);
    }
}
