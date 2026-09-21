#pragma once
#include "Types.h"

struct Vault;

/// An epilogue is the second half of interrupt handling: the part that is
/// allowed to touch kernel data structures. It runs at epilogue level with the
/// Guard held, which is why it receives the Vault directly.
using Epilogue = void (*)(Vault&);

namespace Epilogues {

/// Picks up the keystroke the prologue left in the controller.
void Keyboard(Vault& InVault);

/// Scheduler tick: advances the Bellringer (core 0 only) and preempts.
void Timer(Vault& InVault);

/// Reschedules so a core notices that its thread was killed.
void Assassin(Vault& InVault);

/// Pulls an idle core back out of halt because work became available.
void Wakeup(Vault& InVault);

}  // namespace Epilogues
