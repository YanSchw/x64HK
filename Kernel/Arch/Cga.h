#pragma once
#include "Types.h"

// VGA-compatible text mode. The screen is a plain array of character/attribute
// pairs at a fixed physical address, so printing is just a memory write.
namespace Cga {

constexpr unsigned ROWS = 25;
constexpr unsigned COLUMNS = 80;

/// The framebuffer for text mode lives at a fixed address in the legacy hole.
inline constexpr uintptr_t TEXT_BUFFER_ADDRESS = 0xb8000;

enum class Color : uint8_t {
    BLACK,
    BLUE,
    GREEN,
    CYAN,
    RED,
    MAGENTA,
    BROWN,
    LIGHT_GREY,
    // Only these first eight are available as background colours; the high bit
    // of the background nibble is the blink flag instead.
    DARK_GREY,
    LIGHT_BLUE,
    LIGHT_GREEN,
    LIGHT_CYAN,
    LIGHT_RED,
    LIGHT_MAGENTA,
    YELLOW,
    WHITE,
};

union Attribute {
    struct {
        uint8_t Foreground : 4;
        uint8_t Background : 3;
        uint8_t Blink : 1;
    } __attribute__((packed));
    uint8_t Value;

    explicit constexpr Attribute(Color InForeground = Color::LIGHT_GREY,
                                 Color InBackground = Color::BLACK, bool InBlink = false)
        : Foreground(ToUnderlying(InForeground)), Background(ToUnderlying(InBackground)), Blink(InBlink) {}
} __attribute__((packed));

struct Cell {
    char Character;
    Attribute CellAttribute;

    constexpr Cell(char InCharacter, Attribute InAttribute)
        : Character(InCharacter), CellAttribute(InAttribute) {}
} __attribute__((packed));
static_assert(sizeof(Cell) == 2, "Cga::Cell has the wrong size");

inline Cell* const TEXT_BUFFER = reinterpret_cast<Cell*>(TEXT_BUFFER_ADDRESS);

void SetCursor(unsigned InColumn, unsigned InRow);
void GetCursor(unsigned& OutColumn, unsigned& OutRow);

void Show(unsigned InColumn, unsigned InRow, char InCharacter, Attribute InAttribute = Attribute());
Cell Get(unsigned InColumn, unsigned InRow);

}  // namespace Cga
