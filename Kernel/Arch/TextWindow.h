#pragma once
#include "Types.h"
#include "Arch/Cga.h"

// A rectangular sub-region of the text screen with its own cursor, so several
// producers can write to the screen at once without stepping on each other.
class TextWindow {
public:
    /// Coordinates are absolute screen positions; the upper bounds are exclusive.
    /// Only one window at a time may own the hardware cursor.
    TextWindow(unsigned InFromColumn, unsigned InToColumn, unsigned InFromRow, unsigned InToRow,
               bool InUseHardwareCursor = false);
    ~TextWindow();

    TextWindow(const TextWindow&) = delete;
    TextWindow& operator=(const TextWindow&) = delete;

    void SetPosition(unsigned InColumn, unsigned InRow);

    /// Negative coordinates count back from the right and bottom edges.
    void SetPosition(int InColumn, int InRow);

    void GetPosition(unsigned& OutColumn, unsigned& OutRow) const;

    /// Writes InLength characters (or up to a null byte) at the cursor, wrapping
    /// at the right edge and scrolling the window when it runs past the bottom.
    void Print(const char* InText, Cga::Attribute InAttribute = Cga::Attribute(),
               size_t InLength = SIZE_MAX);

    void Reset(char InFillCharacter = ' ', Cga::Attribute InAttribute = Cga::Attribute());

    /// Moves the hardware cursor into this window.
    void Activate();

protected:
    unsigned GetWidth() const { return m_ToColumn - m_FromColumn; }
    unsigned GetHeight() const { return m_ToRow - m_FromRow; }
    unsigned AbsoluteColumn(unsigned InColumn) const { return m_FromColumn + InColumn; }
    unsigned AbsoluteRow(unsigned InRow) const { return m_FromRow + InRow; }

    void Scroll(Cga::Attribute InAttribute);

    unsigned m_FromColumn = 0;
    unsigned m_ToColumn = 0;
    unsigned m_FromRow = 0;
    unsigned m_ToRow = 0;
    unsigned m_CursorColumn = 0;
    unsigned m_CursorRow = 0;
    bool m_IsActive = false;
};
