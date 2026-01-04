#pragma once

#include "register.hpp"

// Memory addresses for Game Boy cartridges

// NoMBC
static constexpr uint16_t NOMBC_ROM_START = 0x0000;
static constexpr uint16_t NOMBC_ROM_END   = 0x7FFF;

// MBC1 (ROM)
static constexpr uint16_t MBC1_ROM_ROMBANK_X0_START    = 0x0000;
static constexpr uint16_t MBC1_ROM_ROMBANK_X0_END      = 0x3FFF;
static constexpr uint16_t MBC1_ROM_ROMBANK_01_7F_START = 0x4000;
static constexpr uint16_t MBC1_ROM_ROMBANK_01_7F_END   = 0x7FFF;
static constexpr uint16_t MBC1_ROM_RAMBANK_START       = 0xA000;
static constexpr uint16_t MBC1_ROM_RAMBANK_END         = 0xBFFF;

// MBC1 (RAM)
static constexpr uint16_t MBC1_RAM_ENABLE_START      = 0x0000;
static constexpr uint16_t MBC1_RAM_ENABLE_END        = 0x1FFF;
static constexpr uint16_t MBC1_RAM_ROMBANK_START     = 0x2000;
static constexpr uint16_t MBC1_RAM_ROMBANK_END       = 0x3FFF;
static constexpr uint16_t MBC1_RAM_RAMBANK_START     = 0x4000;
static constexpr uint16_t MBC1_RAM_RAMBANK_END       = 0x5FFF;
static constexpr uint16_t MBC1_RAM_BANKINGMODE_START = 0x6000;
static constexpr uint16_t MBC1_RAM_BANKINGMODE_END   = 0x7FFF;

// MBC3 (ROM)
static constexpr uint16_t MBC3_ROM_ROMBANK_00_START       = 0x0000;
static constexpr uint16_t MBC3_ROM_ROMBANK_00_END         = 0x3FFF;
static constexpr uint16_t MBC3_ROM_ROMBANK_01_7F_START    = 0x4000;
static constexpr uint16_t MBC3_ROM_ROMBANK_01_7F_END      = 0x7FFF;
static constexpr uint16_t MBC3_ROM_RAMBANK_REGISTER_START = 0xA000;
static constexpr uint16_t MBC3_ROM_RAMBANK_REGISTER_END   = 0xBFFF;

// MBC3 (RAM)
static constexpr uint16_t MBC3_RAM_ENABLE_TIMER_START = 0x0000;
static constexpr uint16_t MBC3_RAM_ENABLE_TIMER_END   = 0x1FFF;
static constexpr uint16_t MBC3_RAM_ROMBANK_START      = 0x2000;
static constexpr uint16_t MBC3_RAM_ROMBANK_END        = 0x3FFF;
static constexpr uint16_t MBC3_RAM_RAMBANK_START      = 0x4000;
static constexpr uint16_t MBC3_RAM_RAMBANK_END        = 0x5FFF;
static constexpr uint16_t MBC3_RAM_LATCH_CLOCK_START  = 0x6000;
static constexpr uint16_t MBC3_RAM_LATCH_CLOCK_END    = 0x7FFF;

class Address
{
    private:
        uint16_t address;
    public:
        Address(uint16_t location);
        explicit Address(const RegisterPair &from);
        explicit Address(const WordRegister &from);

        uint16_t getValue() const;
        bool inRange(Address low, Address high) const;

        bool operator == (uint16_t other) const;
        Address operator + (uint16_t other) const;
        Address operator - (uint16_t other) const;
};
