#include "Sync/Bellringer.h"
#include "Config.h"
#include "Arch/Apic.h"
#include "Arch/Cpu.h"
#include "Arch/LocalApic.h"
#include "Interrupt/Guard.h"
#include "Debug/Assert.h"

void Bellringer::Check(Vault& InVault) {
    if (m_Bells.IsEmpty()) {
        return;
    }

    Bell* head = m_Bells.First();
    if (head->Ticks > 0) {
        head->Ticks--;
    }

    // Several bells can come due at once: everything following a zero-delta
    // entry was scheduled for the same tick.
    while (!m_Bells.IsEmpty() && m_Bells.First()->Ticks == 0) {
        Bell* bell = m_Bells.Dequeue();
        InVault.Scheduler.Ready(bell->Owner);
    }
}

void Bellringer::Sleep(Vault& InVault, unsigned InMilliseconds) {
    if (InMilliseconds == 0) {
        return;
    }

    const bool wasEmpty = m_Bells.IsEmpty();

    // The bell lives on the sleeping thread's own stack. That is safe precisely
    // because the thread blocks below and its frame therefore stays alive until
    // Check() has dequeued the bell again.
    Bell bell;
    bell.Owner = InVault.Scheduler.Active();
    bell.Ticks = InMilliseconds / Config::SCHEDULER_TICK_MS;
    if (bell.Ticks == 0) {
        bell.Ticks = 1;
    }
    ASSERT(bell.Owner != nullptr);

    // Walk forward subtracting each delta until the remainder is smaller than
    // the next one: that is where this bell belongs.
    Bell* previous = nullptr;
    for (Bell* current = m_Bells.First(); current != nullptr && current->Ticks <= bell.Ticks;
         current = BellQueue::Next(*current)) {
        bell.Ticks -= current->Ticks;
        previous = current;
    }

    // The successor's delta was relative to our new predecessor, so it has to
    // give up the ticks this bell now accounts for.
    Bell* successor = previous == nullptr ? m_Bells.First() : BellQueue::Next(*previous);
    if (successor != nullptr) {
        successor->Ticks -= bell.Ticks;
    }

    if (previous == nullptr) {
        m_Bells.Prepend(bell);
    } else {
        m_Bells.InsertAfter(*previous, bell);
    }

    // Core 0 owns the Bellringer, and an idle core 0 has masked its timer. Poke
    // it so it starts ticking again, otherwise this sleep would never end.
    if (wasEmpty && InVault.Scheduler.IsIdle(0)) {
        LocalApic::Ipi::Send(Apic::GetLocalApicId(0), ToUnderlying(Cpu::Interrupt::Vector::WAKEUP));
    }

    InVault.Scheduler.Resume(false);
}
