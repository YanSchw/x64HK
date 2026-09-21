#pragma once
#include "Types.h"
#include "Lib/OutputStream.h"

// Tees output into two sinks, e.g. screen plus serial line.
class CopyStream : public OutputStream {
public:
    CopyStream(OutputStream* InFirst, OutputStream* InSecond) : m_First(InFirst), m_Second(InSecond) {}

    void Flush() override {
        const char* text = Terminated();
        m_Position = 0;
        if (m_First != nullptr) {
            *m_First << text << ::Flush;
        }
        if (m_Second != nullptr) {
            *m_Second << text << ::Flush;
        }
    }

private:
    OutputStream* m_First;
    OutputStream* m_Second;
};
