#include "Color.hpp"
#include <stdexcept>

namespace
{
// RGB555 colors pack three 5-bit channels: red in bits
// 0-4, green in bits 5-9, and blue in bits 10-14
constexpr uint16_t RGB555_CHANNEL_MASK = 0x1F;
constexpr uint8_t RGB555_GREEN_SHIFT   = 5;
constexpr uint8_t RGB555_BLUE_SHIFT    = 10;

// BCPS/OCPS: bits 0-5 are the byte index into palette memory,
// bit 7 turns on auto-increment, and bit 6 is unused (reads 1)
constexpr uint8_t PALETTE_INDEX_MASK      = 0x3F;
constexpr uint8_t AUTO_INCREMENT_FLAG     = 0x80;
constexpr uint8_t PALETTE_INDEX_UNUSED_BIT = 0x40;
constexpr uint8_t PALETTE_INDEX_WRITABLE  = AUTO_INCREMENT_FLAG | PALETTE_INDEX_MASK;

// Each palette holds 4 colors of 2 bytes each (little-endian).
// Bit 15 of a color is unused.
constexpr uint8_t PALETTE_NUMBER_MASK = 0x07;
constexpr uint8_t COLOR_NUMBER_MASK   = 0x03;
constexpr uint8_t BYTES_PER_COLOR     = 2;
constexpr uint8_t BYTES_PER_PALETTE   = BYTES_PER_COLOR * 4;
constexpr uint8_t COLOR_HIGH_BYTE_MASK = 0x7F;
}

GBColor getColor(uint8_t pixelValue)
{
    switch (pixelValue)
    {
        case 0: return GBColor::Color0;
        case 1: return GBColor::Color1;
        case 2: return GBColor::Color2;
        case 3: return GBColor::Color3;
        default: std::runtime_error("Invalid color code");
    }
    //return GBColor::Color0;
    return {};
}

uint32_t shadeToRGB(Color color)
{
    // White, light gray, dark gray, and black
    static const uint32_t GRAYSCALE[] = {0xFFFFFF, 0xAAAAAA, 0x555555, 0x000000};
    return GRAYSCALE[static_cast<uint8_t>(color) & COLOR_NUMBER_MASK];
}

uint32_t rgb555ToRGB888(uint16_t value)
{
    const uint8_t red = value & RGB555_CHANNEL_MASK;
    const uint8_t green = (value >> RGB555_GREEN_SHIFT) & RGB555_CHANNEL_MASK;
    const uint8_t blue = (value >> RGB555_BLUE_SHIFT) & RGB555_CHANNEL_MASK;

    // Scale each 5-bit channel up to 8 bits, copying
    // the top bits into the bottom so 0x1F maps to 0xFF
    return ((red << 3 | red >> 2) << 16) | ((green << 3 | green >> 2) << 8) | (blue << 3 | blue >> 2);
}

CGBPaletteRAM::CGBPaletteRAM(uint8_t fillValue)
{
    data.fill(fillValue);
}

uint8_t CGBPaletteRAM::readIndex() const
{
    return PALETTE_INDEX_UNUSED_BIT | (index & PALETTE_INDEX_WRITABLE);
}

void CGBPaletteRAM::writeIndex(uint8_t value)
{
    index = value & PALETTE_INDEX_WRITABLE;
}

uint8_t CGBPaletteRAM::readData() const
{
    return data[index & PALETTE_INDEX_MASK];
}

void CGBPaletteRAM::writeData(uint8_t value)
{
    data[index & PALETTE_INDEX_MASK] = value;
    if (index & AUTO_INCREMENT_FLAG) index = static_cast<uint8_t>(AUTO_INCREMENT_FLAG | ((index + 1) & PALETTE_INDEX_MASK));
}

uint32_t CGBPaletteRAM::getColor(uint8_t palette, uint8_t color) const
{
    const uint8_t offset = static_cast<uint8_t>((palette & PALETTE_NUMBER_MASK) * BYTES_PER_PALETTE +
                                                (color & COLOR_NUMBER_MASK) * BYTES_PER_COLOR);
    const uint16_t value = data[offset] | (static_cast<uint16_t>(data[offset + 1] & COLOR_HIGH_BYTE_MASK) << 8);
    return rgb555ToRGB888(value);
}
