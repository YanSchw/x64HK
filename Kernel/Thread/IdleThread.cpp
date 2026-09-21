#include "Thread/IdleThread.h"
#include "Arch/Cpu.h"
#include "Arch/LocalApic.h"
#include "Interrupt/Guard.h"

void IdleThread::Action() {
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
