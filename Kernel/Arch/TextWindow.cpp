#include "Arch/TextWindow.h"

/// There is exactly one hardware cursor, so only one window can own it.
static TextWindow* s_ActiveWindow = nullptr;

TextWindow::TextWindow(unsigned InFromColumn, unsigned InToColumn, unsigned InFromRow, unsigned InToRow,
                       bool InUseHardwareCursor)
    : m_FromColumn(InFromColumn), m_ToColumn(InToColumn), m_FromRow(InFromRow), m_ToRow(InToRow) {
    if (InUseHardwareCursor) {
        Activate();
    }
    Reset();
}

TextWindow::~TextWindow() {
    if (s_ActiveWindow == this) {
        s_ActiveWindow = nullptr;
    }
}

void TextWindow::SetPosition(unsigned InColumn, unsigned InRow) {
    m_CursorColumn = InColumn;
    m_CursorRow = InRow;
    if (m_IsActive) {
        Cga::SetCursor(AbsoluteColumn(m_CursorColumn), AbsoluteRow(m_CursorRow));
    }
}

void TextWindow::SetPosition(int InColumn, int InRow) {
    if (InColumn < 0) {
        InColumn += static_cast<int>(GetWidth());
    }
    if (InRow < 0) {
        InRow += static_cast<int>(GetHeight());
    }
    SetPosition(static_cast<unsigned>(InColumn), static_cast<unsigned>(InRow));
}

void TextWindow::GetPosition(unsigned& OutColumn, unsigned& OutRow) const {
    OutColumn = m_CursorColumn;
    OutRow = m_CursorRow;
}

void TextWindow::Scroll(Cga::Attribute InAttribute) {
    for (unsigned row = 1; row < GetHeight(); row++) {
        for (unsigned column = 0; column < GetWidth(); column++) {
            const Cga::Cell cell = Cga::Get(AbsoluteColumn(column), AbsoluteRow(row));
            Cga::Show(AbsoluteColumn(column), AbsoluteRow(row - 1), cell.Character, cell.CellAttribute);
        }
    }
    for (unsigned column = 0; column < GetWidth(); column++) {
        Cga::Show(AbsoluteColumn(column), AbsoluteRow(GetHeight() - 1), ' ', InAttribute);
    }
}

void TextWindow::Print(const char* InText, Cga::Attribute InAttribute, size_t InLength) {
    for (; *InText != '\0' && InLength > 0; InText++, InLength--) {
        if (*InText == '\n') {
            m_CursorColumn = 0;
            m_CursorRow++;
        } else {
            Cga::Show(AbsoluteColumn(m_CursorColumn), AbsoluteRow(m_CursorRow), *InText, InAttribute);
            if (++m_CursorColumn >= GetWidth()) {
                m_CursorColumn = 0;
                m_CursorRow++;
            }
        }

        if (m_CursorRow >= GetHeight()) {
            Scroll(InAttribute);
            m_CursorRow = GetHeight() - 1;
        }
    }

    SetPosition(m_CursorColumn, m_CursorRow);
}

void TextWindow::Reset(char InFillCharacter, Cga::Attribute InAttribute) {
    for (unsigned row = 0; row < GetHeight(); row++) {
        for (unsigned column = 0; column < GetWidth(); column++) {
            Cga::Show(AbsoluteColumn(column), AbsoluteRow(row), InFillCharacter, InAttribute);
        }
    }
    SetPosition(0u, 0u);
}

void TextWindow::Activate() {
    if (s_ActiveWindow != nullptr) {
        s_ActiveWindow->m_IsActive = false;
    }
    s_ActiveWindow = this;
    m_IsActive = true;
    Cga::SetCursor(AbsoluteColumn(m_CursorColumn), AbsoluteRow(m_CursorRow));
}
