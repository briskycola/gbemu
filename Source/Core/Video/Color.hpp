#pragma once

#include <array>
#include <cstdint>

// How a pixel gets its color, in Pan Docs' terms:
//
// 1. Tile data gives each pixel a 2-bit color index (0-3), kept as a uint8_t.
//    https://gbdev.io/pandocs/Tile_Data.html#data-format
// 2. On the original Game Boy and the Super Game Boy, BGP, OBP0 and OBP1
//    assign a gray shade (Shade) to each color index.
//    https://gbdev.io/pandocs/Palettes.html#ff47--bgp-non-cgb-mode-only-bg-palette-data
// 3. The screen shows a 24-bit RGB color (uint32_t, 0xRRGGBB): the shade's
//    gray, or a 15-bit RGB555 color from the Game Boy Color's or Super Game
//    Boy's palettes.
//    https://gbdev.io/pandocs/Palettes.html#lcd-color-palettes-cgb-only

// The four gray shades, by their 2-bit values
enum class Shade
{
    White,
    LightGray,
    DarkGray,
    Black,
};

// A pixel's color index in a tile row. "The first byte specifies the least
// significant bit of the color ID of each pixel, and the second byte
// specifies the most significant bit. In both bytes, bit 7 represents the
// leftmost pixel, and bit 0 the rightmost."
// https://gbdev.io/pandocs/Tile_Data.html#data-format
uint8_t tileColorIndex(uint8_t low, uint8_t high, uint8_t bit);

// The shade BGP, OBP0 or OBP1 assigns a color index: bits 2N+1 and 2N hold
// the shade of color index N. (OBP0 and OBP1 "work exactly like BGP, except
// that the lower two bits are ignored because color index 0 is transparent
// for OBJs".)
// https://gbdev.io/pandocs/Palettes.html#ff47--bgp-non-cgb-mode-only-bg-palette-data
// https://gbdev.io/pandocs/Palettes.html#ff48ff49--obp0-obp1-non-cgb-mode-only-obj-palette-0-1-data
Shade shadeFromPalette(uint8_t palette, uint8_t colorIndex);

// A shade's 2-bit value, which picks a color from a Super Game Boy palette,
// or a CGB palette in DMG compatibility mode
uint8_t shadeIndex(Shade shade);

// A shade's gray
uint32_t shadeToRGB(Shade shade);

// An RGB555 color (red in bits 0-4, green 5-9, blue 10-14) as 24-bit RGB
uint32_t rgb555ToRGB888(uint16_t value);

// Game Boy Color palette memory. There are two of these: one for the
// background and one for objects. Each holds 8 palettes of 4 colors, 2 bytes
// per color.
//
// The CPU reaches it through an index register (BGPI or OBPI) and a data
// register (BGPD or OBPD), historically known as BCPS, OCPS, BCPD and OCPD.
// If bit 7 of the index is set, the index increments after every data write.
// https://gbdev.io/pandocs/Palettes.html#lcd-color-palettes-cgb-only
class CGBPaletteRAM
{
    private:
        std::array<uint8_t, 64> data{}; // 8 palettes * 4 colors * 2 bytes
        uint8_t index = 0;

    public:
        CGBPaletteRAM(uint8_t fillValue = 0x00);
        uint8_t readIndex() const;
        void writeIndex(uint8_t value);
        uint8_t readData() const;
        void writeData(uint8_t value, bool accessible);
        // A palette's color for a color index, as 24-bit RGB
        uint32_t getColor(uint8_t palette, uint8_t colorIndex) const;
};
