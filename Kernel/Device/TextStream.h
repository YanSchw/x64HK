#pragma once
#include "Types.h"
#include "Arch/TextWindow.h"
#include "Lib/OutputStream.h"

// Formatted output into a region of the text screen.
class TextStream : public OutputStream, public TextWindow {
public:
    TextStream(unsigned InFromColumn, unsigned InToColumn, unsigned InFromRow, unsigned InToRow,
               bool InUseHardwareCursor = false)
        : TextWindow(InFromColumn, InToColumn, InFromRow, InToRow, InUseHardwareCursor) {}

    void Flush() override {
        const char* text = Terminated();
        const size_t length = m_Position;
        m_Position = 0;
        Print(text, m_Attribute, length);
    }

    void SetAttribute(Cga::Attribute InAttribute) { m_Attribute = InAttribute; }

private:
    Cga::Attribute m_Attribute{};
};
