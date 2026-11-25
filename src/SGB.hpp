#pragma once

#include <array>
#include <cstdint>
#include "Common.hpp"

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

        void commandReady();

    public:
        SGB();
        void writeJOYP(uint8_t value);
        uint32_t getColor(uint8_t palette, uint8_t color, uint8_t x, uint8_t y);
        uint8_t getAttribute(uint8_t x, uint8_t y) const;
        // Player 1 reads as 0xF, player 2 as 0xE, and so on
        uint8_t getPlayerId() const { return static_cast<uint8_t>(0x0F - currentPlayer); }
};
