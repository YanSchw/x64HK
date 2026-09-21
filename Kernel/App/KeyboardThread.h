#pragma once
#include "Types.h"
#include "Thread/Thread.h"

// Echoes keystrokes. Blocks on the Vault's semaphore, so it costs nothing while
// no key is pressed.
class KeyboardThread : public Thread {
public:
    void Action() override;
};
