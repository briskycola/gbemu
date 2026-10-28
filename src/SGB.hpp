#pragma once

#include <array>
#include <cstdint>

class SGB
{
    private:
        std::array<uint8_t, 112> command{};
        std::array<uint32_t, 16> palettes{};
        std::array<uint8_t, 20 * 18> attributes{};
        std::array<uint32_t, 160 * 144> previousFrame{};
        uint16_t bitCount = 0;
        bool readyForPulse = true;
        bool readyForWrite = false;
        bool readyForStop = false;
        uint8_t maskMode = 0;
        uint8_t playerCount = 1;
        uint8_t currentPlayer = 0;
        uint8_t previousJoyp = 0xCF;

        void commandReady();
        uint32_t rgb15(uint16_t value) const;

    public:
        SGB();
        void writeJOYP(uint8_t value);
        uint32_t getColor(uint8_t palette, uint8_t color, uint8_t x, uint8_t y);
        uint8_t getAttribute(uint8_t x, uint8_t y) const;
        uint8_t getPlayerId() const { return static_cast<uint8_t>(0x0F - currentPlayer); }
};
