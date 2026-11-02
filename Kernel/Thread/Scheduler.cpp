#include "Thread/Scheduler.h"
#include "Arch/Apic.h"
#include "Arch/Cpu.h"
#include "Arch/LocalApic.h"
#include "Debug/Assert.h"
#include "Debug/Output.h"
#include "Interrupt/Guard.h"
#include "Lib/PerCore.h"

/// Runs on a core with nothing else to do. Never enters the ready queue -- the
/// scheduler falls back to it, which removes the "what if the queue is empty"
/// case from every other code path.
static void IdleLoop(void* InUnused) {
    while (true) {
        // Interrupts off while deciding: otherwise a thread could become ready
        // between the check and the halt, and this core would sleep through it.
        Cpu::Interrupt::Disable();

        if (!Guard::UnsafeVault().Scheduler.IsEmpty()) {
            LocalApic::Timer::SetMasked(false);
            Cpu::Interrupt::Enable();
            Guard::Enter().Vault().Scheduler.Resume();
            continue;
        }

        // Nothing to preempt, so the timer can be silenced -- except on core 0,
        // which still has to tick the Bellringer for any sleeping thread.
        const bool keepTicking =
            Cpu::GetId() == 0 && Guard::UnsafeVault().Bellringer.HasPendingBells();
        LocalApic::Timer::SetMasked(!keepTicking);

        // sti + hlt: the enable takes effect one instruction late, so an
        // interrupt cannot arrive in the gap and leave this core halted.
        Cpu::Idle();
    }
}

/// One idle thread per core; never queued, only ever fallen back to.
static PerCore<Thread> s_IdleThread;

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
    s_IdleThread.Get().Prepare("idle", IdleLoop);
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
