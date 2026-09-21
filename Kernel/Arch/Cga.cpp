#include "Arch/Cga.h"
#include "Arch/IoPort.h"

namespace Cga {

// CRT controller registers are reached through an index/data port pair.
static const IoPort s_IndexPort(0x3d4);
static const IoPort s_DataPort(0x3d5);

enum CrtcRegister : uint8_t {
    CURSOR_LOCATION_HIGH = 14,
    CURSOR_LOCATION_LOW = 15,
};

static void WriteCrtc(uint8_t InRegister, uint8_t InValue) {
    s_IndexPort.OutB(InRegister);
    s_DataPort.OutB(InValue);
}

static uint8_t ReadCrtc(uint8_t InRegister) {
    s_IndexPort.OutB(InRegister);
    return s_DataPort.InB();
}

static constexpr unsigned ToOffset(unsigned InColumn, unsigned InRow) {
    return InRow * COLUMNS + InColumn;
}

void SetCursor(unsigned InColumn, unsigned InRow) {
    const uint16_t offset = static_cast<uint16_t>(ToOffset(InColumn, InRow));
    WriteCrtc(CURSOR_LOCATION_HIGH, (offset >> 8) & 0xff);
    WriteCrtc(CURSOR_LOCATION_LOW, offset & 0xff);
}

void GetCursor(unsigned& OutColumn, unsigned& OutRow) {
    const uint16_t offset = static_cast<uint16_t>(ReadCrtc(CURSOR_LOCATION_HIGH) << 8) |
                            ReadCrtc(CURSOR_LOCATION_LOW);
    OutColumn = offset % COLUMNS;
    OutRow = offset / COLUMNS;
}

void Show(unsigned InColumn, unsigned InRow, char InCharacter, Attribute InAttribute) {
    if (InColumn >= COLUMNS || InRow >= ROWS) {
        return;
    }
    TEXT_BUFFER[ToOffset(InColumn, InRow)] = Cell(InCharacter, InAttribute);
}

Cell Get(unsigned InColumn, unsigned InRow) {
    if (InColumn >= COLUMNS || InRow >= ROWS) {
        return Cell(' ', Attribute());
    }
    return TEXT_BUFFER[ToOffset(InColumn, InRow)];
}

}  // namespace Cga
