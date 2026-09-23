#pragma once
#include "Types.h"
#include "Thread/Thread.h"

// Runs on a core with nothing else to do. Never enters the ready queue -- the
// scheduler falls back to it, which removes the "what if the queue is empty"
// case from every other code path.
class IdleThread : public Thread {
public:
    void Action() override;
    const char* Name() const override { return "idle"; }
};
