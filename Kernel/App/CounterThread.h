#pragma once
#include "Types.h"
#include "Arch/Cga.h"

// Prints a counter at a fixed row of the shared window. Two of these on a
// multi-core system show preemption and migration: the counters keep running
// and the core they report changes.
struct Counter {
    const char* Label;
    Cga::Attribute Attribute;
    unsigned Row;
    uint64_t Value = 0;
};

void CounterThread(void* InCounter);
