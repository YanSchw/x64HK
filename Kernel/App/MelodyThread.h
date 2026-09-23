#pragma once
#include "Types.h"
#include "Thread/Thread.h"

// Plays a short tune on the PC speaker, which exercises the Bellringer and
// proves that a blocked thread really does give its core away.
class MelodyThread : public Thread {
public:
    void Action() override;
    const char* Name() const override { return "melody"; }

private:
    struct Note {
        uint32_t Frequency;  ///< Hz, 0 for a rest
        unsigned Milliseconds;
    };
};
