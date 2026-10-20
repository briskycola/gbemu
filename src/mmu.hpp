#pragma once

#include "address.hpp"
#include "register.hpp"
#include <vector>

// https://gbdev.io/pandocs/Memory_Map.html

// Constants to represent start and end addresses
// for each section of memory.
static constexpr uint16_t CARTRIDGE_ROM_ADDRESS_START  = 0x0000;
static constexpr uint16_t CARTRIDGE_ROM_ADDRESS_END    = 0x7FFF;
static constexpr uint16_t VRAM_ADDRESS_START           = 0x8000;
static constexpr uint16_t VRAM_ADDRESS_END             = 0x9FFF;
static constexpr uint16_t CARTRIDGE_RAM_ADDRESS_START  = 0xA000;
static constexpr uint16_t CARTRIDGE_RAM_ADDRESS_END    = 0xBFFF;
static constexpr uint16_t WRAM_ADDRESS_START           = 0xC000;
static constexpr uint16_t WRAM_ADDRESS_END             = 0xDFFF;
static constexpr uint16_t ECHO_RAM_ADDRESS_START       = 0xE000;
static constexpr uint16_t ECHO_RAM_ADDRESS_END         = 0xFDFF;
static constexpr uint16_t OAM_ADDRESS_START            = 0xFE00;
static constexpr uint16_t OAM_ADDRESS_END              = 0xFE9F;
static constexpr uint16_t UNUSABLE_ADDRESS_START       = 0xFEA0;
static constexpr uint16_t UNUSABLE_ADDRESS_END         = 0xFEFF;
static constexpr uint16_t IO_ADDRESS_START             = 0xFF00;
static constexpr uint16_t IO_ADDRESS_END               = 0xFF7F;
static constexpr uint16_t HRAM_ADDRESS_START           = 0xFF80;
static constexpr uint16_t HRAM_ADDRESS_END             = 0xFFFE;
static constexpr uint16_t INTERRUPT_ENABLE_REGISTER    = 0xFFFF;

// Offsets of work, oam, and high RAM
static constexpr uint16_t WRAM_OFFSET     = 0xC000;
static constexpr uint16_t ECHO_RAM_OFFSET = 0x2000;
static constexpr uint16_t OAM_OFFSET      = 0xFE00;
static constexpr uint16_t HRAM_OFFSET     = 0xFF80;

// Sizes of work, oam, and high RAM
static constexpr uint16_t WRAM_SIZE_BYTES = 0x8000;
static constexpr uint16_t OAM_SIZE_BYTES  = 0xA0;
static constexpr uint16_t HRAM_SIZE_BYTES = 0x80;
static constexpr uint16_t VRAM_SIZE_BYTES = 0x4000;

// Offsets for DMA
static constexpr uint16_t DMA_SOURCE_PAGE_SHIFT = 0x100;
static constexpr uint16_t DMA_TRANSFER_LENGTH   = 0x9F;

class Emulator;

class MMU
{
    private:
        Emulator &emulator;
        std::vector<uint8_t> workRAM;
        std::vector<uint8_t> oamRAM;
        std::vector<uint8_t> highRAM;
        ByteRegister disableBootROMSwitch;

        uint8_t readIO(const Address &address) const;
        void writeIO(const Address &address, const uint8_t value);
        void dmaTransfer(uint8_t value);
        bool isBootROMActive() const;
    public:
        MMU(Emulator &emulator);
        uint8_t read(const Address &address, bool bypassPPURestrictions = false) const;
        void write(const Address &address, const uint8_t value, bool bypassPPURestrictions = false);
};
