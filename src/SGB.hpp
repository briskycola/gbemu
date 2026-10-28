#pragma once

#include <array>
#include <cstdint>
#include "Common.hpp"

// https://gbdev.io/pandocs/SGB_Functions.html

// Commands arrive as 1 to 7 packets of 16 bytes each
static constexpr uint8_t SGB_PACKET_BYTES = 16;
static constexpr uint16_t SGB_PACKET_BITS = SGB_PACKET_BYTES * 8;
static constexpr uint8_t SGB_MAX_PACKETS  = 7;

// Color attributes are set per 8x8 tile, so the screen is
// a 20x18 grid of cells each using one of 4 palettes
static constexpr uint8_t SGB_ATTRIBUTE_COLUMNS = GAMEBOY_WIDTH / 8;
static constexpr uint8_t SGB_ATTRIBUTE_ROWS    = GAMEBOY_HEIGHT / 8;
static constexpr uint8_t SGB_PALETTE_COUNT     = 4;
static constexpr uint8_t SGB_COLORS_PER_PALETTE = 4;

class SGB
{
    private:
        std::array<uint8_t, SGB_PACKET_BYTES * SGB_MAX_PACKETS> command{};
        std::array<uint32_t, SGB_PALETTE_COUNT * SGB_COLORS_PER_PALETTE> palettes{};
        std::array<uint8_t, SGB_ATTRIBUTE_COLUMNS * SGB_ATTRIBUTE_ROWS> attributes{};
        std::array<uint32_t, GAMEBOY_WIDTH * GAMEBOY_HEIGHT> previousFrame{};
        uint16_t bitCount = 0;
        bool readyForPulse = true;
        bool readyForWrite = false;
        bool readyForStop = false;
        uint8_t maskMode = 0;
        uint8_t playerCount = 1;
        uint8_t currentPlayer = 0;
        uint8_t previousJoyp = 0xCF; // JOYP with nothing selected or pressed

        void commandReady();

    public:
        SGB();
        void writeJOYP(uint8_t value);
        uint32_t getColor(uint8_t palette, uint8_t color, uint8_t x, uint8_t y);
        uint8_t getAttribute(uint8_t x, uint8_t y) const;
        // Player 1 reads as 0xF, player 2 as 0xE, and so on
        uint8_t getPlayerId() const { return static_cast<uint8_t>(0x0F - currentPlayer); }
};
