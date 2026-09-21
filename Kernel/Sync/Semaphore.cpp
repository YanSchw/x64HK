#include "Sync/Semaphore.h"
#include "Interrupt/Guard.h"
#include "Debug/Assert.h"

// No atomics here on purpose: every caller is inside the Guard's critical
// section, so these operations are already serialised system wide.

void Semaphore::P(Vault& InVault) {
    if (m_Counter > 0) {
        m_Counter--;
        return;
    }

    Thread* current = InVault.Scheduler.Active();
    ASSERT(current != nullptr);
    m_Waiting.Append(*current);

    // false: do not put the caller back on the ready queue -- it is parked here
    // until a V() hands it over.
    InVault.Scheduler.Resume(false);
}

void Semaphore::V(Vault& InVault) {
    Thread* waiter = m_Waiting.Dequeue();
    if (waiter != nullptr) {
        InVault.Scheduler.Ready(waiter);
    } else {
        m_Counter++;
    }
}
