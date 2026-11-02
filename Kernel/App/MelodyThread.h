#pragma once
#include "Types.h"

// Plays a short tune on the PC speaker, which exercises the Bellringer and
// proves that a blocked thread really does give its core away.
void MelodyThread(void* InUnused);
