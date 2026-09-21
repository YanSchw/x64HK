#pragma once
#include "Types.h"
#include "Thread/Thread.h"

// Echoes keystrokes into a row of its own in the Vault's shared window. Blocks
// on the Vault's semaphore, so it costs nothing while no key is pressed.
class KeyboardThread : public Thread {
public:
    void Action() override;

private:
    unsigned m_Column = 0;
};
