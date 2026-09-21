#include "Interrupt/Guard.h"
#include "Arch/Cga.h"
#include "Arch/Cpu.h"
#include "Lib/PerCore.h"
#include "Lib/RingBuffer.h"
#include "Sync/TicketLock.h"
#include "Debug/Assert.h"

Vault::Vault() : Output(0, Cga::COLUMNS, 0, 4, false) {}

static Vault s_Vault;

/// One lock for the whole system: at most one epilogue runs anywhere at a time.
/// Ticket based so a busy core cannot starve the others.
static TicketLock s_GlobalLock;

/// Per core, because a prologue can only be blocked by an epilogue on its own
/// core -- the other cores are handled by the lock above.
static PerCore<RingBuffer<Epilogue, Config::EPILOGUE_QUEUE_SIZE>> s_EpilogueQueue;
static PerCore<bool> s_InEpilogue;

Guarded::~Guarded() {
    Guard::Leave();
}

Guarded Guard::Enter() {
    {
        // The flag has to be set atomically with respect to this core's
        // prologues, hence the interrupt guard rather than the lock.
        Cpu::Interrupt::Guard interruptGuard;
        s_InEpilogue.Set(true);
    }

    s_GlobalLock.Lock();
    return Guarded(s_Vault);
}

void Guard::Leave() {
    const bool wasEnabled = Cpu::Interrupt::IsEnabled();

    s_GlobalLock.Unlock();

    // Drain whatever prologues queued while we held the level. Each epilogue
    // runs with interrupts on, so new ones can arrive during this loop.
    while (true) {
        Cpu::Interrupt::Disable();

        Epilogue next = nullptr;
        if (!s_EpilogueQueue->Consume(next)) {
            break;
        }

        Cpu::Interrupt::Enable();
        s_GlobalLock.Lock();
        next(s_Vault);
        s_GlobalLock.Unlock();
    }

    // Interrupts are off here, which is what makes clearing the flag safe
    // against a prologue observing a stale value.
    s_InEpilogue.Set(false);
    Cpu::Interrupt::Restore(wasEnabled);
}

void Guard::Relay(Epilogue InEpilogue) {
    const bool wasEnabled = Cpu::Interrupt::Disable();

    const bool levelBusy = s_InEpilogue.Get();
    const bool queued = s_EpilogueQueue->Produce(InEpilogue);
    ASSERT(queued);

    if (levelBusy) {
        // Somebody on this core is already inside; they will pick it up on
        // their way out.
        Cpu::Interrupt::Restore(wasEnabled);
        return;
    }

    // The level is free, so this prologue takes it and runs the epilogue
    // itself. Leave() both drains the queue and clears the flag.
    s_InEpilogue.Set(true);
    Cpu::Interrupt::Restore(wasEnabled);

    s_GlobalLock.Lock();
    Leave();
}

const Vault& Guard::UnsafeVault() {
    return s_Vault;
}
