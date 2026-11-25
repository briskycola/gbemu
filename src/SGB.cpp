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
constexpr uint8_t COMMAND_PAL_SET   = 0x0A; // Set palettes 0-3 from system palettes
constexpr uint8_t COMMAND_PAL_TRN   = 0x0B; // Transfer the 512 system palettes
constexpr uint8_t COMMAND_ICON_EN   = 0x0E; // Enable or disable SGB functions
constexpr uint8_t COMMAND_MLT_REQ   = 0x11; // Request multiplayer mode
constexpr uint8_t COMMAND_ATTR_TRN  = 0x15; // Transfer the 45 attribute files
constexpr uint8_t COMMAND_ATTR_SET  = 0x16; // Apply an attribute file
constexpr uint8_t COMMAND_MASK_EN   = 0x17; // Freeze or blank the screen

// The game sends bits by writing the JOYP select lines
// (bits 4-5): both low resets, both high ends a pulse,
// and exactly one low sends a 1 or a 0
constexpr uint8_t PULSE_RESET = 0;
constexpr uint8_t PULSE_ONE   = 1;
constexpr uint8_t PULSE_ZERO  = 2;
constexpr uint8_t PULSE_IDLE  = 3;

// PAL_SET's flags byte and ATTR_SET's file byte
constexpr uint8_t PAL_SET_FLAGS          = 9;
constexpr uint8_t PAL_SET_APPLY_FILE_BIT = 7;
constexpr uint8_t CANCEL_MASK_BIT        = 6;
constexpr uint8_t ATTRIBUTE_FILE_MASK    = 0x3F;

// ICON_EN bit 2 stops the SGB taking any more packets
constexpr uint8_t ICON_EN_DISABLE_PACKETS_BIT = 2;

// MASK_EN modes
constexpr uint8_t MASK_CANCEL = 0;
constexpr uint8_t MASK_FREEZE = 1; // Keep showing the last frame
constexpr uint8_t MASK_BLACK  = 2;
constexpr uint8_t MASK_COLOR0 = 3; // Fill with palette 0's color 0

// Attribute cells cover 8x8 pixels
constexpr uint8_t ATTRIBUTE_CELL_SIZE = 8;

// The longest data list that fits after a command's 2 header bytes
constexpr uint8_t MAX_ATTRIBUTE_LINES   = sgb::SGB_PACKET_BYTES * sgb::SGB_MAX_PACKETS - 2;
constexpr uint8_t ATTRIBUTE_BLOCK_BYTES = 6;
constexpr uint8_t MAX_ATTRIBUTE_BLOCKS  = MAX_ATTRIBUTE_LINES / ATTRIBUTE_BLOCK_BYTES;

// The default palettes are shades of gray (RGB555):
// white, light gray, dark gray, and black
constexpr uint16_t DEFAULT_PALETTE[sgb::SGB_COLORS_PER_PALETTE] = {0x7FFF, 0x56B5, 0x294A, 0x0000};

// Little-endian RGB555 color stored at command[offset]
uint32_t readColor(const std::array<uint8_t, sgb::SGB_PACKET_BYTES * sgb::SGB_MAX_PACKETS> &command, uint8_t offset)
{
    return rgb555ToRGB888(command[offset] | (command[offset + 1] << 8));
}
}

SGB::SGB()
{
    for (uint8_t p = 0; p < sgb::SGB_PALETTE_COUNT; ++p)
        for (uint8_t c = 0; c < sgb::SGB_COLORS_PER_PALETTE; ++c)
            palettes[p * sgb::SGB_COLORS_PER_PALETTE + c] = rgb555ToRGB888(DEFAULT_PALETTE[c]);
    attributes.fill(0);
    previousFrame.fill(shadeToRGB(Color::White));
}

void SGB::writeJOYP(uint8_t value)
{
    const uint8_t select = (value & input::JOYPAD_SELECT_MASK) >> input::JOYPAD_SELECT_DIRECTION_BIT;

    // In multiplayer mode, a rising edge on the button select
    // line moves on to the next player's controller
    if (checkBit(value, input::JOYPAD_SELECT_BUTTONS_BIT) && !checkBit(previousJoyp, input::JOYPAD_SELECT_BUTTONS_BIT) && playerCount > 1)
        currentPlayer = (currentPlayer + 1) & (playerCount - 1);
    previousJoyp = value;
    if (select == PULSE_IDLE) { readyForPulse = true; return; }

    if (select == PULSE_RESET)
    {
        if (readyForPulse)
        {
            readyForWrite = true;
            readyForPulse = false;
            if ((bitCount % sgb::SGB_PACKET_BITS) != 0 || bitCount == 0)
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
        // The boot ROM's header packets are single packets whatever their
        // low bits say
        const bool headerPacket = command[0] >= sgb::HEADER_PACKET_FIRST && (command[0] & 1);
        const uint16_t packets = headerPacket || !(command[0] & PACKET_COUNT_MASK) ? 1 : (command[0] & PACKET_COUNT_MASK);
        if (bitCount >= packets * sgb::SGB_PACKET_BITS)
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
    if ((bitCount % sgb::SGB_PACKET_BITS) == 0) readyForStop = true;
}

void SGB::commandReady()
{
    // The boot ROM's header packets
    if (command[0] >= sgb::HEADER_PACKET_FIRST && command[0] <= sgb::HEADER_PACKET_LAST && (command[0] & 1))
    {
        const uint8_t start = static_cast<uint8_t>((command[0] - sgb::HEADER_PACKET_FIRST) / 2 * sgb::HEADER_BYTES_PER_PACKET);
        for (uint8_t i = 0; i < sgb::HEADER_BYTES_PER_PACKET && start + i < sgb::HEADER_BYTES; ++i)
            header[start + i] = command[sgb::HEADER_PACKET_DATA + i];
        if (command[0] == sgb::HEADER_PACKET_LAST)
            functionsUnlocked = header[sgb::HEADER_SGB_FLAG - sgb::HEADER_START] == sgb::SGB_FLAG_VALUE &&
                header[sgb::HEADER_OLD_LICENSEE - sgb::HEADER_START] == sgb::OLD_LICENSEE_SGB;
        return;
    }

    // Games that don't unlock the SGB functions can't use its commands, and
    // after ICON_EN disables packets the SGB takes no more
    if (!functionsUnlocked || packetsDisabled) return;

    const uint8_t op = command[0] >> COMMAND_SHIFT;

    // ICON_EN: bit 2 stops the SGB taking any more packets. (Bits 0 and 1
    // only affect the SGB's own menus, which aren't emulated.)
    // https://gbdev.io/pandocs/SGB_Command_System.html#sgb-command-0e--icon_en
    if (op == COMMAND_ICON_EN)
    {
        packetsDisabled = checkBit(command[1], ICON_EN_DISABLE_PACKETS_BIT);
        return;
    }

    // PAL_TRN and ATTR_TRN send their data through a VRAM transfer
    if (op == COMMAND_PAL_TRN || op == COMMAND_ATTR_TRN)
    {
        startTransfer(op);
        return;
    }

    // PAL_SET: bytes 1-8 are four system palette IDs (little-endian) for
    // palettes 0-3. The flags byte can also apply an attribute file (bit 7,
    // file in bits 0-5) and cancel MASK_EN (bit 6).
    // https://gbdev.io/pandocs/SGB_Command_Palettes.html#sgb-command-0a--pal_set
    if (op == COMMAND_PAL_SET)
    {
        for (uint8_t p = 0; p < sgb::SGB_PALETTE_COUNT; ++p)
        {
            const uint16_t id = (command[1 + p * 2] | (command[2 + p * 2] << 8)) & sgb::SYSTEM_PALETTE_ID_MASK;
            for (uint8_t c = 0; c < sgb::SGB_COLORS_PER_PALETTE; ++c)
                palettes[p * sgb::SGB_COLORS_PER_PALETTE + c] = rgb555ToRGB888(systemPalettes[id * sgb::SGB_COLORS_PER_PALETTE + c]);
            // Color 0 is shared, and the last one set wins
            setColor0(palettes[p * sgb::SGB_COLORS_PER_PALETTE]);
        }
        const uint8_t flags = command[PAL_SET_FLAGS];
        if (checkBit(flags, PAL_SET_APPLY_FILE_BIT)) applyAttributeFile(flags & ATTRIBUTE_FILE_MASK);
        if (checkBit(flags, CANCEL_MASK_BIT)) maskMode = MASK_CANCEL;
        return;
    }

    // ATTR_SET: apply attribute file bits 0-5 of byte 1; bit 6 cancels MASK_EN
    // https://gbdev.io/pandocs/SGB_Command_Attribute.html#sgb-command-16--attr_set
    if (op == COMMAND_ATTR_SET)
    {
        applyAttributeFile(command[1] & ATTRIBUTE_FILE_MASK);
        if (checkBit(command[1], CANCEL_MASK_BIT)) maskMode = MASK_CANCEL;
        return;
    }


    // PAL01, PAL23, PAL03, and PAL12: set two palettes at once.
    // Bytes 1-2 are the color 0 shared by every palette, bytes 3-8
    // the first palette's colors 1-3, and bytes 9-14 the second's.
    if (op <= COMMAND_PAL12)
    {
        const uint8_t first = op == COMMAND_PAL01 ? 0 : (op == COMMAND_PAL23 ? 2 : (op == COMMAND_PAL03 ? 0 : 1));
        const uint8_t second = op == COMMAND_PAL01 ? 1 : (op == COMMAND_PAL23 ? 3 : (op == COMMAND_PAL03 ? 3 : 2));
        const uint32_t color0 = readColor(command, 1);
        setColor0(color0);
        for (uint8_t i = 0; i < sgb::SGB_COLORS_PER_PALETTE - 1; ++i)
        {
            palettes[first * sgb::SGB_COLORS_PER_PALETTE + i + 1] = readColor(command, 3 + i * 2);
            palettes[second * sgb::SGB_COLORS_PER_PALETTE + i + 1] = readColor(command, 9 + i * 2);
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
                if (line < sgb::SGB_ATTRIBUTE_ROWS)
                    for (uint8_t x = 0; x < sgb::SGB_ATTRIBUTE_COLUMNS; ++x) attributes[line * sgb::SGB_ATTRIBUTE_COLUMNS + x] = palette;
            }
            else if (line < sgb::SGB_ATTRIBUTE_COLUMNS)
                for (uint8_t y = 0; y < sgb::SGB_ATTRIBUTE_ROWS; ++y) attributes[y * sgb::SGB_ATTRIBUTE_COLUMNS + line] = palette;
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
        for (uint8_t y = 0; y < sgb::SGB_ATTRIBUTE_ROWS; ++y) for (uint8_t x = 0; x < sgb::SGB_ATTRIBUTE_COLUMNS; ++x)
        {
            const uint8_t coord = (mode & 0x40) ? y : x;
            attributes[y * sgb::SGB_ATTRIBUTE_COLUMNS + x] = coord < line ? low : (coord == line ? middle : high);
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
        for (uint16_t i = 0; i < count && i < sgb::SGB_ATTRIBUTE_COLUMNS * sgb::SGB_ATTRIBUTE_ROWS; ++i)
        {
            const uint8_t palette = (command[6 + i / 4] >> (((~i) & 3) * 2)) & 3;
            if (x >= sgb::SGB_ATTRIBUTE_COLUMNS || y >= sgb::SGB_ATTRIBUTE_ROWS) break;
            attributes[y * sgb::SGB_ATTRIBUTE_COLUMNS + x] = palette;
            if (vertical) { if (++y == sgb::SGB_ATTRIBUTE_ROWS) { y = 0; if (++x == sgb::SGB_ATTRIBUTE_COLUMNS) break; } }
            else { if (++x == sgb::SGB_ATTRIBUTE_COLUMNS) { x = 0; if (++y == sgb::SGB_ATTRIBUTE_ROWS) break; } }
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
            for (uint8_t y = 0; y < sgb::SGB_ATTRIBUTE_ROWS; ++y) for (uint8_t x = 0; x < sgb::SGB_ATTRIBUTE_COLUMNS; ++x)
            {
                const bool outside = x < left || x > right || y < top || y > bottom;
                const bool inside = x > left && x < right && y > top && y < bottom;
                const bool edge = !outside && !inside;
                if (outside && changeOutside) attributes[y * sgb::SGB_ATTRIBUTE_COLUMNS + x] = outsidePalette;
                else if (inside && changeInside) attributes[y * sgb::SGB_ATTRIBUTE_COLUMNS + x] = insidePalette;
                else if (edge && changeEdge) attributes[y * sgb::SGB_ATTRIBUTE_COLUMNS + x] = edgePalette;
            }
        }
        return;
    }

    if (op == COMMAND_MASK_EN) maskMode = command[1] & 3;
}


// Color 0 is shared by every palette: it is the SNES backdrop, and whichever
// command set it last wins
// https://gbdev.io/pandocs/SGB_Color_Palettes.html#color-0-restriction
void SGB::setColor0(uint32_t rgb)
{
    for (uint8_t p = 0; p < sgb::SGB_PALETTE_COUNT; ++p) palettes[p * sgb::SGB_COLORS_PER_PALETTE] = rgb;
}

// An attribute file holds 18 rows of 5 bytes, 2 bits per cell, the leftmost
// cell in the top bits. Files past $2C don't exist.
// https://gbdev.io/pandocs/SGB_Command_Attribute.html#sgb-command-15--attr_trn
void SGB::applyAttributeFile(uint8_t file)
{
    if (file >= sgb::ATTRIBUTE_FILES) return;
    const uint8_t *data = &attributeFiles[file * sgb::ATTRIBUTE_FILE_BYTES];
    for (uint8_t y = 0; y < sgb::SGB_ATTRIBUTE_ROWS; ++y)
        for (uint8_t x = 0; x < sgb::SGB_ATTRIBUTE_COLUMNS; ++x)
        {
            const uint8_t byte = data[y * sgb::ATTRIBUTE_FILE_ROW_BYTES + x / 4];
            attributes[y * sgb::SGB_ATTRIBUTE_COLUMNS + x] = (byte >> ((3 - x % 4) * 2)) & 3;
        }
}

// A _TRN command starts a VRAM transfer with the next frame
// https://gbdev.io/pandocs/SGB_VRAM_Transfer.html#transfer-time
void SGB::startTransfer(uint8_t op)
{
    transferCommand = op;
    transferFrames = 0;
    transferPending = true;
}

void SGB::frameFinished(const FrameBuffer &frame)
{
    if (!transferPending) return;
    ++transferFrames;
    if (transferFrames == sgb::TRANSFER_READ_FRAME + 1) readTransfer(frame);
    if (transferFrames == sgb::TRANSFER_DONE_FRAME + 1)
    {
        transferPending = false;
        finishTransfer();
    }
}

// The SNES reads the 4 KiB from the picture, rebuilding the tile data as it
// was in VRAM: tile n is the 8x8 cell at column n % 20, row n / 20, and each
// pixel's shade (BGP is $E4) gives its bit in the low and high bytes of its row
// https://gbdev.io/pandocs/SGB_VRAM_Transfer.html#transfer-data
void SGB::readTransfer(const FrameBuffer &frame)
{
    for (uint16_t tile = 0; tile < sgb::TRANSFER_TILES; ++tile)
    {
        const uint32_t left = (tile % sgb::TRANSFER_TILES_PER_ROW) * ATTRIBUTE_CELL_SIZE;
        const uint32_t top = (tile / sgb::TRANSFER_TILES_PER_ROW) * ATTRIBUTE_CELL_SIZE;
        for (uint8_t row = 0; row < ATTRIBUTE_CELL_SIZE; ++row)
        {
            uint8_t low = 0, high = 0;
            for (uint8_t column = 0; column < ATTRIBUTE_CELL_SIZE; ++column)
            {
                const uint8_t shade = static_cast<uint8_t>(frame.getPixel(left + column, top + row));
                low = static_cast<uint8_t>(low << 1 | (shade & 1));
                high = static_cast<uint8_t>(high << 1 | (shade >> 1));
            }
            transferData[tile * 16 + row * 2] = low;
            transferData[tile * 16 + row * 2 + 1] = high;
        }
    }
}

void SGB::finishTransfer()
{
    // PAL_TRN: 512 palettes of 4 little-endian RGB555 colors
    if (transferCommand == COMMAND_PAL_TRN)
    {
        for (uint16_t i = 0; i < systemPalettes.size(); ++i)
            systemPalettes[i] = static_cast<uint16_t>(transferData[i * 2] | (transferData[i * 2 + 1] << 8));
    }
    // ATTR_TRN: 45 attribute files of 90 bytes
    else if (transferCommand == COMMAND_ATTR_TRN)
    {
        std::copy_n(transferData.begin(), attributeFiles.size(), attributeFiles.begin());
    }
}
uint32_t SGB::getColor(uint8_t palette, uint8_t color, uint8_t x, uint8_t y)
{
    const size_t index = y * common::GAMEBOY_WIDTH + x;
    if (maskMode == MASK_FREEZE) return previousFrame[index];
    uint32_t rgb = maskMode == MASK_BLACK ? shadeToRGB(Color::Black) :
        (maskMode == MASK_COLOR0 ? palettes[0]
                                : palettes[(palette % sgb::SGB_PALETTE_COUNT) * sgb::SGB_COLORS_PER_PALETTE + (color % sgb::SGB_COLORS_PER_PALETTE)]);
    previousFrame[index] = rgb;
    return rgb;
}

uint8_t SGB::getAttribute(uint8_t x, uint8_t y) const
{
    const uint8_t row = std::min<uint8_t>(y / ATTRIBUTE_CELL_SIZE, sgb::SGB_ATTRIBUTE_ROWS - 1);
    const uint8_t column = std::min<uint8_t>(x / ATTRIBUTE_CELL_SIZE, sgb::SGB_ATTRIBUTE_COLUMNS - 1);
    return attributes[row * sgb::SGB_ATTRIBUTE_COLUMNS + column];
}
