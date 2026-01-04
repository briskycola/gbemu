#include "common.hpp"

Cycles::Cycles(uint32_t cycles) :
    cycles(cycles)
{}

uint8_t composeBits(const uint8_t high, const uint8_t low)
{
    return static_cast<uint8_t>(high << 1 | low);
}

uint8_t composeNibbles(const uint8_t high, const uint8_t low)
{
    return static_cast<uint8_t>(high << 4 | low);
}

uint8_t composeBytes(const uint8_t high, const uint8_t low)
{
    return static_cast<uint16_t>(high << 8 | low);
}

bool checkBit(const uint8_t value, const uint8_t bit)
{
    return (value & (1 << bit)) != 0;
}

uint8_t getBitValue(const uint8_t value, const uint8_t bit)
{
    return (value >> bit) & 1;
}

uint8_t setBit(const uint8_t value, const uint8_t bit)
{
    return static_cast<uint8_t>(value | (1 << bit));
}

uint8_t clearBit(const uint8_t value, const uint8_t bit)
{
    return static_cast<uint8_t>(value & ~(1 << bit));
}

uint8_t setBitTo(const uint8_t value, const uint8_t bit, const bool set)
{
    return set ? setBit(value, bit) : clearBit(value, bit);
}
