#pragma once
#include "Types.h"
#include "Config.h"
#include "Device/Key.h"
#include "Device/TextStream.h"
#include "Interrupt/Epilogue.h"
#include "Lib/RingBuffer.h"
#include "Sync/Bellringer.h"
#include "Sync/Semaphore.h"
#include "Thread/Scheduler.h"

/// Everything that interrupt handlers and threads share. Reaching any of it
/// requires going through the Guard, so holding a Vault reference is the
/// evidence that the critical section is held.
struct Vault {
    Vault();

    Vault(const Vault&) = delete;
    Vault& operator=(const Vault&) = delete;

    class Scheduler Scheduler;
    class Bellringer Bellringer;
    TextStream Output;
    RingBuffer<Key, Config::KEY_BUFFER_SIZE> Keys;
    class Semaphore KeysAvailable;
};

/// RAII handle for the critical section. Leaving its scope runs any epilogues
/// that piled up in the meantime.
class Guarded {
public:
    explicit Guarded(Vault& InVault) : m_Vault(InVault) {}
    ~Guarded();

    Guarded(const Guarded&) = delete;
    Guarded& operator=(const Guarded&) = delete;

    Vault& Vault() { return m_Vault; }
    const struct Vault& Vault() const { return m_Vault; }

private:
    struct Vault& m_Vault;
};

// Prologue/epilogue synchronisation.
//
// Interrupt handling is split in two. The prologue runs immediately, with
// interrupts disabled, and does only what the hardware demands -- acknowledge
// the device, grab the volatile bit of state. Everything that touches kernel
// data structures is deferred into an epilogue.
//
// Epilogues run at "epilogue level", which is entered either by a thread
// (Guard::Enter) or, if the level is free, by the prologue itself. Two things
// protect it:
//
//   * a per-core flag, which stops a prologue on this core from re-entering
//     the level, and
//   * one global lock, because a per-core flag says nothing about what the
//     other cores are doing and only one epilogue may run system wide.
//
// Interrupts are re-enabled while an epilogue runs. That is the whole point of
// the model: a prologue may interrupt an epilogue and simply queue its own.
namespace Guard {

/// Enters the critical section from thread level, blocking until it is free.
Guarded Enter();

/// Leaves the critical section and drains the epilogue queue. Normally called
/// by ~Guarded rather than directly.
void Leave();

/// Called from a prologue to request that its epilogue be run. Runs it right
/// away if the level is free, otherwise queues it for whoever is in there.
void Relay(Epilogue InEpilogue);

/// Reads the Vault without taking the lock. Only valid for cheap, tolerant
/// checks such as "is the ready queue empty".
const Vault& UnsafeVault();

}  // namespace Guard
