#include "App/CounterThread.h"
#include "Arch/Cpu.h"
#include "Interrupt/Guard.h"

void CounterThread(void* InCounter) {
    Counter& counter = *static_cast<Counter*>(InCounter);

    while (true) {
        {
            Guarded guard = Guard::Enter();
            Vault& vault = guard.Vault();

            vault.Output.SetAttribute(counter.Attribute);
            vault.Output.SetPosition(0u, counter.Row);
            vault.Output << counter.Label << ' ' << Dec << counter.Value++ << " on core "
                         << Cpu::GetId() << "   " << Flush;
        }

        Guarded guard = Guard::Enter();
        guard.Vault().Bellringer.Sleep(guard.Vault(), 1000);
    }
}
