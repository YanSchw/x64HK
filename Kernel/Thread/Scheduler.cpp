#include "Thread/Scheduler.h"
#include "Arch/Apic.h"
#include "Arch/Cpu.h"
#include "Arch/LocalApic.h"
#include "Debug/Assert.h"
#include "Debug/Output.h"
#include "Lib/PerCore.h"

/// One idle thread per core; never queued, only ever fallen back to.
static PerCore<IdleThread> s_IdleThread;

Thread* Scheduler::GetNext() {
    while (Thread* next = m_ReadyQueue.Dequeue()) {
        if (!next->IsDying()) {
            return next;
        }
    }

    return &s_IdleThread.Get();
}

void Scheduler::Schedule() {
    // Idle threads never pass through Ready, and this is the one point every
    // core reaches before it could ever fall back to one.
    s_IdleThread.Get().Prepare();
    m_Dispatcher.Go(GetNext());
}

void Scheduler::Ready(Thread* InThread) {
    ASSERT(InThread != nullptr);
    InThread->Prepare();
    m_ReadyQueue.Append(*InThread);

    // Wake one sleeping core so the new work is picked up without waiting for
    // the next timer tick.
    //
    // The calling core is skipped deliberately. It is awake and about to
    // reschedule anyway -- and because an epilogue runs on the stack of the
    // thread it interrupted, this core reads as idle here whenever the idle
    // thread was the one interrupted. Poking it would consume the wake-up and
    // leave every other core asleep.
    const unsigned self = Cpu::GetId();
    for (unsigned core = 0; core < Cpu::Count(); core++) {
        if (core != self && IsIdle(core)) {
            LocalApic::Ipi::Send(Apic::GetLocalApicId(core),
                                 ToUnderlying(Cpu::Interrupt::Vector::WAKEUP));
            break;
        }
    }
}

void Scheduler::Resume(bool InRequeue) {
    Thread* current = m_Dispatcher.Active();
    ASSERT(current != nullptr);

    // The idle thread is never queued: it is what the core falls back to.
    if (InRequeue && !current->IsDying() && current != &s_IdleThread.Get()) {
        m_ReadyQueue.Append(*current);
    }

    m_Dispatcher.Dispatch(GetNext());
}

void Scheduler::Exit() {
    m_Dispatcher.Dispatch(GetNext());
}

void Scheduler::Kill(Thread* InThread) {
    ASSERT(InThread != nullptr);
    InThread->MarkDying();

    // If it is only queued, dropping it here is enough.
    if (m_ReadyQueue.Remove(*InThread)) {
        return;
    }

    // Otherwise it may be running on another core, which has to be interrupted
    // to notice the flag.
    for (unsigned core = 0; core < Cpu::Count(); core++) {
        if (IsActive(InThread, core)) {
            LocalApic::Ipi::Send(Apic::GetLocalApicId(core),
                                 ToUnderlying(Cpu::Interrupt::Vector::ASSASSIN));
            break;
        }
    }
}

bool Scheduler::IsIdle(unsigned InCoreId) {
    return IsActive(&s_IdleThread[InCoreId], InCoreId);
}
