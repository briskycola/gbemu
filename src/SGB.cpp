#include "SGB.hpp"
#include "Color.hpp"
#include "Input.hpp"
#include <algorithm>

namespace
{
// Command codes, stored in the upper 5 bits of a command's first
// byte. The lower 3 bits hold the number of packets it spans.
constexpr uint8_t COMMAND_SHIFT    = 3;
constexpr uint8_t PACKET_COUNT_MASK = 0x07;
constexpr uint8_t COMMAND_PAL01    = 0x00; // Set palettes 0 and 1
constexpr uint8_t COMMAND_PAL23    = 0x01; // Set palettes 2 and 3
constexpr uint8_t COMMAND_PAL03    = 0x02; // Set palettes 0 and 3
constexpr uint8_t COMMAND_PAL12    = 0x03; // Set palettes 1 and 2
constexpr uint8_t COMMAND_ATTR_BLK  = 0x04; // Color rectangular blocks
constexpr uint8_t COMMAND_ATTR_LIN  = 0x05; // Color whole rows or columns
constexpr uint8_t COMMAND_ATTR_DIV  = 0x06; // Split the screen in three along a line
constexpr uint8_t COMMAND_ATTR_CHR  = 0x07; // Color individual cells
constexpr uint8_t COMMAND_MLT_REQ   = 0x11; // Request multiplayer mode
constexpr uint8_t COMMAND_MASK_EN   = 0x17; // Freeze or blank the screen

// The game sends bits by writing the JOYP select lines
// (bits 4-5): both low resets, both high ends a pulse,
// and exactly one low sends a 1 or a 0
constexpr uint8_t PULSE_RESET = 0;
constexpr uint8_t PULSE_ONE   = 1;
constexpr uint8_t PULSE_ZERO  = 2;
constexpr uint8_t PULSE_IDLE  = 3;

// MASK_EN modes
constexpr uint8_t MASK_FREEZE = 1; // Keep showing the last frame
constexpr uint8_t MASK_BLACK  = 2;
constexpr uint8_t MASK_COLOR0 = 3; // Fill with palette 0's color 0

// Attribute cells cover 8x8 pixels
constexpr uint8_t ATTRIBUTE_CELL_SIZE = 8;

// The longest data list that fits after a command's 2 header bytes
constexpr uint8_t MAX_ATTRIBUTE_LINES   = SGB_PACKET_BYTES * SGB_MAX_PACKETS - 2;
constexpr uint8_t ATTRIBUTE_BLOCK_BYTES = 6;
constexpr uint8_t MAX_ATTRIBUTE_BLOCKS  = MAX_ATTRIBUTE_LINES / ATTRIBUTE_BLOCK_BYTES;

// The default palettes are shades of gray (RGB555):
// white, light gray, dark gray, and black
constexpr uint16_t DEFAULT_PALETTE[SGB_COLORS_PER_PALETTE] = {0x7FFF, 0x56B5, 0x294A, 0x0000};

// Little-endian RGB555 color stored at command[offset]
uint32_t readColor(const std::array<uint8_t, SGB_PACKET_BYTES * SGB_MAX_PACKETS> &command, uint8_t offset)
{
    return rgb555ToRGB888(command[offset] | (command[offset + 1] << 8));
}
}

SGB::SGB()
{
    for (uint8_t p = 0; p < SGB_PALETTE_COUNT; ++p)
        for (uint8_t c = 0; c < SGB_COLORS_PER_PALETTE; ++c)
            palettes[p * SGB_COLORS_PER_PALETTE + c] = rgb555ToRGB888(DEFAULT_PALETTE[c]);
    attributes.fill(0);
    previousFrame.fill(shadeToRGB(Color::White));
}

void SGB::writeJOYP(uint8_t value)
{
    const uint8_t select = (value & JOYPAD_SELECT_MASK) >> JOYPAD_SELECT_DIRECTION_BIT;

    // In multiplayer mode, a rising edge on the button select
    // line moves on to the next player's controller
    if (checkBit(value, JOYPAD_SELECT_BUTTONS_BIT) && !checkBit(previousJoyp, JOYPAD_SELECT_BUTTONS_BIT) && playerCount > 1)
        currentPlayer = (currentPlayer + 1) & (playerCount - 1);
    previousJoyp = value;
    if (select == PULSE_IDLE) { readyForPulse = true; return; }

    if (select == PULSE_RESET)
    {
        if (readyForPulse)
        {
            readyForWrite = true;
            readyForPulse = false;
            if ((bitCount % SGB_PACKET_BITS) != 0 || bitCount == 0)
            {
                bitCount = 0;
                command.fill(0);
                readyForStop = false;
            }
        }
        return;
    }

    if (!readyForPulse || !readyForWrite) return;
    readyForPulse = false;

    // Each packet ends with a 0 bit (the stop bit)
    if (readyForStop)
    {
        if (select != PULSE_ZERO)
        {
            bitCount = 0;
            command.fill(0);
            readyForStop = false;
            readyForWrite = false;
            return;
        }
        const uint16_t packets = (command[0] & PACKET_COUNT_MASK) ? (command[0] & PACKET_COUNT_MASK) : 1;
        if (bitCount >= packets * SGB_PACKET_BITS)
        {
            commandReady();
            bitCount = 0;
            command.fill(0);
        }
        readyForStop = false;
        readyForWrite = false;
        return;
    }

    // Bits arrive least significant first
    if (bitCount < command.size() * 8 && select == PULSE_ONE)
        command[bitCount / 8] |= static_cast<uint8_t>(1u << (bitCount % 8));
    if (bitCount < command.size() * 8) ++bitCount;
    if ((bitCount % SGB_PACKET_BITS) == 0) readyForStop = true;
}

void SGB::commandReady()
{
    const uint8_t op = command[0] >> COMMAND_SHIFT;

    // PAL01, PAL23, PAL03, and PAL12: set two palettes at once.
    // Bytes 1-2 are the color 0 shared by every palette, bytes 3-8
    // the first palette's colors 1-3, and bytes 9-14 the second's.
    if (op <= COMMAND_PAL12)
    {
        const uint8_t first = op == COMMAND_PAL01 ? 0 : (op == COMMAND_PAL23 ? 2 : (op == COMMAND_PAL03 ? 0 : 1));
        const uint8_t second = op == COMMAND_PAL01 ? 1 : (op == COMMAND_PAL23 ? 3 : (op == COMMAND_PAL03 ? 3 : 2));
        const uint32_t color0 = readColor(command, 1);
        for (uint8_t p = 0; p < SGB_PALETTE_COUNT; ++p) palettes[p * SGB_COLORS_PER_PALETTE] = color0;
        for (uint8_t i = 0; i < SGB_COLORS_PER_PALETTE - 1; ++i)
        {
            palettes[first * SGB_COLORS_PER_PALETTE + i + 1] = readColor(command, 3 + i * 2);
            palettes[second * SGB_COLORS_PER_PALETTE + i + 1] = readColor(command, 9 + i * 2);
        }
        return;
    }

    // ATTR_LIN: byte 1 is the number of entries. In each entry, bits 0-4
    // are the row/column, bits 5-6 the palette, and bit 7 picks a row.
    if (op == COMMAND_ATTR_LIN)
    {
        const uint8_t count = std::min<uint8_t>(command[1], MAX_ATTRIBUTE_LINES);
        for (uint8_t i = 0; i < count; ++i)
        {
            const uint8_t data = command[2 + i];
            const uint8_t palette = (data >> 5) & 3;
            const uint8_t line = data & 0x1F;
            if (data & 0x80)
            {
                if (line < SGB_ATTRIBUTE_ROWS)
                    for (uint8_t x = 0; x < SGB_ATTRIBUTE_COLUMNS; ++x) attributes[line * SGB_ATTRIBUTE_COLUMNS + x] = palette;
            }
            else if (line < SGB_ATTRIBUTE_COLUMNS)
                for (uint8_t y = 0; y < SGB_ATTRIBUTE_ROWS; ++y) attributes[y * SGB_ATTRIBUTE_COLUMNS + line] = palette;
        }
        return;
    }

    // ATTR_DIV: byte 1 holds the palettes for cells after the line
    // (bits 0-1), before it (bits 2-3), and on it (bits 4-5); bit 6
    // makes the line horizontal. Byte 2 is the line's position.
    if (op == COMMAND_ATTR_DIV)
    {
        const uint8_t mode = command[1];
        const uint8_t low = (mode >> 2) & 3;
        const uint8_t middle = (mode >> 4) & 3;
        const uint8_t high = mode & 3;
        const uint8_t line = command[2] & 0x1F;
        for (uint8_t y = 0; y < SGB_ATTRIBUTE_ROWS; ++y) for (uint8_t x = 0; x < SGB_ATTRIBUTE_COLUMNS; ++x)
        {
            const uint8_t coord = (mode & 0x40) ? y : x;
            attributes[y * SGB_ATTRIBUTE_COLUMNS + x] = coord < line ? low : (coord == line ? middle : high);
        }
        return;
    }

    // ATTR_CHR: bytes 1-2 are the starting cell, bytes 3-4 the number
    // of cells, and bit 0 of byte 5 fills down columns instead of
    // across rows. From byte 6, each byte holds four 2-bit palettes.
    if (op == COMMAND_ATTR_CHR)
    {
        uint8_t x = command[1], y = command[2];
        const uint16_t count = command[3] | (command[4] << 8);
        const bool vertical = (command[5] & 1) != 0;
        for (uint16_t i = 0; i < count && i < SGB_ATTRIBUTE_COLUMNS * SGB_ATTRIBUTE_ROWS; ++i)
        {
            const uint8_t palette = (command[6 + i / 4] >> (((~i) & 3) * 2)) & 3;
            if (x >= SGB_ATTRIBUTE_COLUMNS || y >= SGB_ATTRIBUTE_ROWS) break;
            attributes[y * SGB_ATTRIBUTE_COLUMNS + x] = palette;
            if (vertical) { if (++y == SGB_ATTRIBUTE_ROWS) { y = 0; if (++x == SGB_ATTRIBUTE_COLUMNS) break; } }
            else { if (++x == SGB_ATTRIBUTE_COLUMNS) { x = 0; if (++y == SGB_ATTRIBUTE_ROWS) break; } }
        }
        return;
    }

    // MLT_REQ: byte 1 requests 1 (0), 2 (1), or 4 (3) players
    if (op == COMMAND_MLT_REQ)
    {
        const uint8_t request = command[1] & 3;
        playerCount = request == 0 ? 1 : (request == 1 ? 2 : (request == 3 ? 4 : 1));
        currentPlayer &= playerCount - 1;
        return;
    }

    // ATTR_BLK: byte 1 is the number of 6-byte blocks. Each block has
    // a control byte (bits 0-2 change the inside, border, and outside),
    // a byte with those three palettes, and the left, top, right, and
    // bottom cells of the rectangle.
    if (op == COMMAND_ATTR_BLK)
    {
        const uint8_t count = std::min<uint8_t>(command[1], MAX_ATTRIBUTE_BLOCKS);
        for (uint8_t i = 0; i < count; ++i)
        {
            const uint8_t *block = &command[2 + i * ATTRIBUTE_BLOCK_BYTES];
            const uint8_t control = block[0];
            const uint8_t insidePalette = block[1] & 3;
            const uint8_t middlePalette = (block[1] >> 2) & 3;
            const uint8_t outsidePalette = (block[1] >> 4) & 3;
            const bool changeInside = (control & 1) != 0;
            bool changeEdge = (control & 2) != 0;
            const bool changeOutside = (control & 4) != 0;

            // If only the inside or only the outside changes,
            // the border takes that palette too
            uint8_t edgePalette = middlePalette;
            if (changeInside && !changeEdge && !changeOutside) { changeEdge = true; edgePalette = insidePalette; }
            else if (changeOutside && !changeEdge && !changeInside) { changeEdge = true; edgePalette = outsidePalette; }
            const uint8_t left = block[2] & 0x1F, top = block[3] & 0x1F;
            const uint8_t right = block[4] & 0x1F, bottom = block[5] & 0x1F;
            for (uint8_t y = 0; y < SGB_ATTRIBUTE_ROWS; ++y) for (uint8_t x = 0; x < SGB_ATTRIBUTE_COLUMNS; ++x)
            {
                const bool outside = x < left || x > right || y < top || y > bottom;
                const bool inside = x > left && x < right && y > top && y < bottom;
                const bool edge = !outside && !inside;
                if (outside && changeOutside) attributes[y * SGB_ATTRIBUTE_COLUMNS + x] = outsidePalette;
                else if (inside && changeInside) attributes[y * SGB_ATTRIBUTE_COLUMNS + x] = insidePalette;
                else if (edge && changeEdge) attributes[y * SGB_ATTRIBUTE_COLUMNS + x] = edgePalette;
            }
        }
        return;
    }

    if (op == COMMAND_MASK_EN) maskMode = command[1] & 3;
}

uint32_t SGB::getColor(uint8_t palette, uint8_t color, uint8_t x, uint8_t y)
{
    const size_t index = y * GAMEBOY_WIDTH + x;
    if (maskMode == MASK_FREEZE) return previousFrame[index];
    uint32_t rgb = maskMode == MASK_BLACK ? shadeToRGB(Color::Black) :
        (maskMode == MASK_COLOR0 ? palettes[0]
                                : palettes[(palette % SGB_PALETTE_COUNT) * SGB_COLORS_PER_PALETTE + (color % SGB_COLORS_PER_PALETTE)]);
    previousFrame[index] = rgb;
    return rgb;
}

uint8_t SGB::getAttribute(uint8_t x, uint8_t y) const
{
    const uint8_t row = std::min<uint8_t>(y / ATTRIBUTE_CELL_SIZE, SGB_ATTRIBUTE_ROWS - 1);
    const uint8_t column = std::min<uint8_t>(x / ATTRIBUTE_CELL_SIZE, SGB_ATTRIBUTE_COLUMNS - 1);
    return attributes[row * SGB_ATTRIBUTE_COLUMNS + column];
}
