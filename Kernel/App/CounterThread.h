#pragma once
#include "Types.h"
#include "Arch/Cga.h"
#include "Thread/Thread.h"

// Prints a counter at a fixed screen position. Two of these on a multi-core
// system show preemption and migration: the counters keep running and the core
// they report changes.
class CounterThread : public Thread {
public:
    CounterThread(const char* InName, Cga::Attribute InAttribute, unsigned InRow)
        : m_Name(InName), m_Attribute(InAttribute), m_Row(InRow) {}

    void Action() override;
    const char* Name() const override { return m_Name; }

private:
    const char* m_Name;
    Cga::Attribute m_Attribute;
    unsigned m_Row;
    uint64_t m_Counter = 0;
};
