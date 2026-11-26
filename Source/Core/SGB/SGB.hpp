#pragma once

#include <array>
#include <cstdint>
#include "Core/Common/Common.hpp"
#include "Core/Video/FrameBuffer.hpp"

// https://gbdev.io/pandocs/SGB_Functions.html

// Commands arrive as 1 to 7 packets of 16 bytes each
namespace sgb
{
    static constexpr uint8_t SGB_PACKET_BYTES = 16;
    static constexpr uint16_t SGB_PACKET_BITS = SGB_PACKET_BYTES * 8;
    static constexpr uint8_t SGB_MAX_PACKETS  = 7;
    
    // Color attributes are set per 8x8 tile, so the screen is
    // a 20x18 grid of cells each using one of 4 palettes
    static constexpr uint8_t SGB_ATTRIBUTE_COLUMNS  = common::GAMEBOY_WIDTH / 8;
    static constexpr uint8_t SGB_ATTRIBUTE_ROWS     = common::GAMEBOY_HEIGHT / 8;
    static constexpr uint8_t SGB_PALETTE_COUNT      = 4;
    static constexpr uint8_t SGB_COLORS_PER_PALETTE = 4;

    // The SGB boot ROM sends the cartridge header ($0104-$014F) in packets
    // $F1, $F3 ... $FB, each holding the packet ID, a checksum and 14 header
    // bytes (6 in the last). The SNES program unlocks the SGB functions if
    // the SGB flag is $03 and the old licensee code $33.
    // https://gbdev.io/pandocs/Power_Up_Sequence.html#super-game-boy-sgb-sgb2
    // https://gbdev.io/pandocs/SGB_Unlocking.html#cartridge-header
    static constexpr uint16_t HEADER_START           = 0x0104;
    static constexpr uint8_t HEADER_BYTES            = 0x0150 - HEADER_START;
    static constexpr uint8_t HEADER_PACKET_FIRST     = 0xF1;
    static constexpr uint8_t HEADER_PACKET_LAST      = 0xFB;
    static constexpr uint8_t HEADER_PACKET_DATA      = 2;  // Offset of the header bytes in a packet
    static constexpr uint8_t HEADER_BYTES_PER_PACKET = 14;
    static constexpr uint16_t HEADER_SGB_FLAG        = 0x0146;
    static constexpr uint16_t HEADER_OLD_LICENSEE    = 0x014B;
    static constexpr uint8_t SGB_FLAG_VALUE          = 0x03;
    static constexpr uint8_t OLD_LICENSEE_SGB        = 0x33;

    // VRAM transfers: the SNES reads 4 KiB from the Game Boy's picture, which
    // must show tiles $00-$FF in order, 20 per row. The data is read from the
    // first frame after the command's, and the transfer is done at the end of
    // the 5th.
    // https://gbdev.io/pandocs/SGB_VRAM_Transfer.html
    static constexpr uint16_t TRANSFER_BYTES          = 0x1000;
    static constexpr uint16_t TRANSFER_TILES          = 256;
    static constexpr uint8_t TRANSFER_TILES_PER_ROW   = 20;
    static constexpr uint8_t TRANSFER_READ_FRAME      = 1;
    static constexpr uint8_t TRANSFER_DONE_FRAME      = 5;

    // System palettes (PAL_TRN) and attribute files (ATTR_TRN)
    // https://gbdev.io/pandocs/SGB_Command_Palettes.html#sgb-command-0b--pal_trn
    // https://gbdev.io/pandocs/SGB_Command_Attribute.html#sgb-command-15--attr_trn
    static constexpr uint16_t SYSTEM_PALETTES         = 512;
    static constexpr uint16_t SYSTEM_PALETTE_ID_MASK  = 0x1FF;
    static constexpr uint8_t ATTRIBUTE_FILES          = 45;
    static constexpr uint8_t ATTRIBUTE_FILE_BYTES     = 90; // 18 rows of 5 bytes, 2 bits per cell
    static constexpr uint8_t ATTRIBUTE_FILE_ROW_BYTES = 5;

    // The border: a 256x224 SNES picture with the Game Boy screen in the
    // middle. CHR_TRN loads 128 of its 256 4bpp tiles at a time, PCT_TRN its
    // 32x28 tile map (plus a 29th row) and palettes 4-6 of 16 colors.
    // https://gbdev.io/pandocs/SGB_Command_Border.html
    static constexpr uint16_t BORDER_WIDTH            = 256;
    static constexpr uint16_t BORDER_HEIGHT           = 224;
    static constexpr uint16_t GAME_LEFT               = (BORDER_WIDTH - common::GAMEBOY_WIDTH) / 2;
    static constexpr uint16_t GAME_TOP                = (BORDER_HEIGHT - common::GAMEBOY_HEIGHT) / 2;
    static constexpr uint16_t BORDER_TILES            = 256;
    static constexpr uint8_t BORDER_TILE_BYTES        = 32;
    static constexpr uint8_t BORDER_MAP_COLUMNS       = 32;
    static constexpr uint8_t BORDER_MAP_ROWS          = 29;
    static constexpr uint16_t BORDER_PALETTE_DATA     = 0x800;
    static constexpr uint8_t BORDER_FIRST_PALETTE     = 4;
    static constexpr uint8_t BORDER_PALETTE_COUNT     = 3;
    static constexpr uint8_t SNES_PALETTES            = 8;
    static constexpr uint8_t SNES_COLORS_PER_PALETTE  = 16;
}

class SGB
{
    private:
        std::array<uint8_t, sgb::SGB_PACKET_BYTES * sgb::SGB_MAX_PACKETS> command{};
        std::array<uint32_t, sgb::SGB_PALETTE_COUNT * sgb::SGB_COLORS_PER_PALETTE> palettes{};
        std::array<uint8_t, sgb::SGB_ATTRIBUTE_COLUMNS * sgb::SGB_ATTRIBUTE_ROWS> attributes{};
        std::array<uint32_t, common::GAMEBOY_WIDTH * common::GAMEBOY_HEIGHT> previousFrame{};
        uint16_t bitCount = 0;
        bool readyForPulse = true;
        bool readyForWrite = false;
        bool readyForStop = false;
        uint8_t maskMode = 0;
        uint8_t playerCount = 1;
        uint8_t currentPlayer = 0;
        uint8_t previousJoyp = 0xCF; // JOYP with nothing selected or pressed

        // The cartridge header the boot ROM sends, and whether it unlocks the
        // SGB functions
        std::array<uint8_t, sgb::HEADER_BYTES> header{};
        bool functionsUnlocked = false;

        // A VRAM transfer in progress: its command, and frames since it was sent
        uint8_t transferCommand = 0;
        uint8_t transferFrames = 0;
        bool transferPending = false;
        bool transferHighTiles = false; // CHR_TRN loads tiles $80-$FF
        std::array<uint8_t, sgb::TRANSFER_BYTES> transferData{};

        std::array<uint16_t, sgb::SYSTEM_PALETTES * sgb::SGB_COLORS_PER_PALETTE> systemPalettes{}; // RGB555
        std::array<uint8_t, sgb::ATTRIBUTE_FILES * sgb::ATTRIBUTE_FILE_BYTES> attributeFiles{};
        bool packetsDisabled = false; // ICON_EN bit 2

        // The border's tiles, tile map, palettes, and the picture it makes
        std::array<uint8_t, sgb::BORDER_TILES * sgb::BORDER_TILE_BYTES> borderTiles{};
        std::array<uint16_t, sgb::BORDER_MAP_COLUMNS * sgb::BORDER_MAP_ROWS> borderMap{};
        std::array<uint32_t, sgb::SNES_PALETTES * sgb::SNES_COLORS_PER_PALETTE> borderPalettes{};
        bool borderLoaded = false;
        FrameBuffer screen{sgb::BORDER_WIDTH, sgb::BORDER_HEIGHT};

        void commandReady();
        void startTransfer(uint8_t op);
        void readTransfer(const FrameBuffer &frame);
        void finishTransfer();
        void applyAttributeFile(uint8_t file);
        void setColor0(uint32_t rgb);
        uint32_t readColor(uint8_t offset) const;

    public:
        SGB();
        void writeJOYP(uint8_t value);
        // Called with every finished frame, which VRAM transfers read
        void frameFinished(const FrameBuffer &frame);

        // Whether the game has sent a border, and the 256x224 picture of the
        // border around this frame of the game
        bool hasBorder() const { return borderLoaded; }
        const FrameBuffer &compose(const FrameBuffer &game);
        // The color shown for a shade at (x, y) with the given attribute palette,
        // unless MASK_EN freezes the screen or blanks it to black or color 0
        uint32_t getColor(uint8_t palette, Shade shade, uint8_t x, uint8_t y);
        uint8_t getAttribute(uint8_t x, uint8_t y) const;
        // Player 1 reads as 0xF, player 2 as 0xE, and so on
        uint8_t getPlayerId() const { return static_cast<uint8_t>(0x0F - currentPlayer); }
};
