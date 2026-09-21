#include "App/MelodyThread.h"
#include "Arch/Pit.h"
#include "Interrupt/Guard.h"

void MelodyThread::Action() {
    static constexpr Note MELODY[] = {
        {262, 300}, {0, 60},   {330, 300}, {0, 60},  {392, 300}, {0, 60},
        {523, 450}, {0, 100},  {392, 300}, {0, 60},  {523, 600}, {0, 1500},
    };

    while (true) {
        continue;  // TODO: Remove this when the melody is ready to be played.

        for (const Note& note : MELODY) {
            Guarded guard = Guard::Enter();

            // The speaker is driven by PIT channel 2, so this has to be inside
            // the critical section -- the LAPIC calibration uses it too.
            Pit::PcSpeaker(note.Frequency);
            guard.Vault().Bellringer.Sleep(guard.Vault(), note.Milliseconds);
        }

        Guarded guard = Guard::Enter();
        Pit::PcSpeaker(0);
        guard.Vault().Bellringer.Sleep(guard.Vault(), 5000);
    }
}
