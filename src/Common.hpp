#pragma once

#include <cstdint>

namespace common
{
    static constexpr uint8_t GAMEBOY_WIDTH        = 160;
    static constexpr uint8_t GAMEBOY_HEIGHT       = 144;
    static constexpr uint16_t BACKGROUND_MAP_SIZE = 256;
    static constexpr int32_t GAMEBOY_CLOCK_RATE   = 4194304; // T-cycles (clocks) per second
    // The (NTSC) Super Game Boy runs off the SNES clock: 21.477 MHz / 5, 2.41% fast
    // https://gbdev.io/pandocs/SGB_Functions.html#sgb-clock-speed
    static constexpr int32_t SGB_CLOCK_RATE       = 4295454;
    
    // The CPU counts in machine cycles (M-cycles), each
    // of which is 4 clock cycles (T-cycles)
    static constexpr uint32_t T_CYCLES_PER_M_CYCLE = 4;
}

// Make a base type that is designed
// to only allow creation and not
// copying.
//
// This will be the base type for a lot
// of our objects
class NoCopy
{
    public:
        NoCopy() = default;
        ~NoCopy() = default;
        NoCopy(const NoCopy&) = delete;
        NoCopy &operator = (const NoCopy&) = delete;
};

// Some useful bitwise operations
uint8_t composeBits(const uint8_t high, const uint8_t low);
uint8_t composeNibbles(const uint8_t high, const uint8_t low);
uint16_t composeBytes(const uint8_t high, const uint8_t low);
bool checkBit(const uint8_t value, const uint8_t bit);
uint8_t getBitValue(const uint8_t value, const uint8_t bit);
uint8_t setBit(const uint8_t value, const uint8_t bit);
uint8_t clearBit(const uint8_t value, const uint8_t bit);
uint8_t setBitTo(const uint8_t value, const uint8_t bit, const bool set);

// Make a class to represent the number
// of CPU cycles
class Cycles
{
    public:
        const uint32_t cycles;
        Cycles(uint32_t cycles);
};
