#pragma once

#include <array>
#include <cstdint>

// Colors on the original Game Boy are one
// of four shades of gray. Tiles store a 2-bit
// color number per pixel (GBColor), which a
// palette register maps to a shade (Color).
//
// The Game Boy Color (and Super Game Boy) use
// 15-bit RGB colors instead, stored as
// 0bxBBBBBGGGGGRRRRR.
//
// https://gbdev.io/pandocs/Palettes.html

enum class GBColor
{
    Color0, // White
    Color1, // Light gray
    Color2, // Dark gray
    Color3, // Black
};

enum class Color
{
    White,
    LightGray,
    DarkGray,
    Black,
};

class ColorPalette
{
    public:
        Color color0 = Color::White;
        Color color1 = Color::LightGray;
        Color color2 = Color::DarkGray;
        Color color3 = Color::Black;
};

// Game Boy Color palette memory. There are two
// of these: one for the background and one for
// sprites. Each holds 8 palettes of 4 colors,
// 2 bytes per color.
//
// The CPU accesses it through an index register
// (BCPS/OCPS) and a data register (BCPD/OCPD).
// If bit 7 of the index is set, the index
// increments after every data write.
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
        void writeData(uint8_t value);
        uint32_t getColor(uint8_t palette, uint8_t color) const;
};

GBColor getColor(uint8_t pixelValue);
uint32_t shadeToRGB(Color color);
uint32_t rgb555ToRGB888(uint16_t value);
