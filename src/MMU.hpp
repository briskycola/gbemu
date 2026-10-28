#pragma once

#include "Address.hpp"
#include "Register.hpp"
#include <vector>

// https://gbdev.io/pandocs/Memory_Map.html

// Constants to represent start and end addresses
// for each section of memory.
static constexpr uint16_t CARTRIDGE_ROM_ADDRESS_START = 0x0000;
static constexpr uint16_t CARTRIDGE_ROM_ADDRESS_END   = 0x7FFF;
static constexpr uint16_t VRAM_ADDRESS_START         = 0x8000;
static constexpr uint16_t VRAM_ADDRESS_END           = 0x9FFF;
static constexpr uint16_t CARTRIDGE_RAM_ADDRESS_START = 0xA000;
static constexpr uint16_t CARTRIDGE_RAM_ADDRESS_END   = 0xBFFF;
static constexpr uint16_t WRAM_ADDRESS_START         = 0xC000;
static constexpr uint16_t WRAM_ADDRESS_END           = 0xDFFF;
static constexpr uint16_t ECHO_RAM_ADDRESS_START      = 0xE000;
static constexpr uint16_t ECHO_RAM_ADDRESS_END        = 0xFDFF;
static constexpr uint16_t OAM_ADDRESS_START          = 0xFE00;
static constexpr uint16_t OAM_ADDRESS_END            = 0xFE9F;
static constexpr uint16_t UNUSABLE_ADDRESS_START     = 0xFEA0;
static constexpr uint16_t UNUSABLE_ADDRESS_END       = 0xFEFF;
static constexpr uint16_t IO_ADDRESS_START           = 0xFF00;
static constexpr uint16_t IO_ADDRESS_END             = 0xFF7F;
static constexpr uint16_t HRAM_ADDRESS_START         = 0xFF80;
static constexpr uint16_t HRAM_ADDRESS_END           = 0xFFFE;
static constexpr uint16_t INTERRUPT_ENABLE_REGISTER  = 0xFFFF;

// Offsets of work, oam, and high RAM
static constexpr uint16_t WRAM_OFFSET    = 0xC000;
static constexpr uint16_t ECHO_RAM_OFFSET = 0x2000;
static constexpr uint16_t OAM_OFFSET     = 0xFE00;
static constexpr uint16_t HRAM_OFFSET    = 0xFF80;

// Sizes of work, oam, and high RAM
static constexpr uint16_t WRAM_SIZE_BYTES = 0x8000;
static constexpr uint16_t OAM_SIZE_BYTES  = 0xA0;
static constexpr uint16_t HRAM_SIZE_BYTES = 0x80;
static constexpr uint16_t VRAM_SIZE_BYTES = 0x4000;

// VRAM is a single 8 KB window at 0x8000. The Game
// Boy Color has two banks of it, selected through VBK.
static constexpr uint16_t VRAM_BANK_SIZE = 0x2000;

// Work RAM is split into 4 KB banks. Bank 0 is fixed at
// 0xC000, and on the Game Boy Color 0xD000 can hold any
// of banks 1-7 (selected through SVBK).
static constexpr uint16_t WRAM_BANK_SIZE = 0x1000;
static constexpr uint8_t WRAM_BANK_MASK  = 0x07;

// The DMG boot ROM covers 0x0000-0x00FF. The CGB boot ROM
// also covers 0x0200-0x08FF, leaving the cartridge header
// at 0x0100-0x01FF visible while it runs.
static constexpr uint16_t DMG_BOOT_ROM_SIZE            = 0x100;
static constexpr uint16_t CGB_BOOT_ROM_SECOND_PART_START = 0x200;
static constexpr uint16_t CGB_BOOT_ROM_SIZE            = 0x900;

// OAM DMA copies 160 bytes (0x00-0x9F) from page
// (value * 0x100) into OAM, stalling the CPU for
// 160 machine cycles
static constexpr uint16_t DMA_SOURCE_PAGE_SHIFT = 0x100;
static constexpr uint16_t DMA_TRANSFER_LENGTH  = 0x9F;
static constexpr uint16_t OAM_DMA_CYCLES       = 160;

// Game Boy Color HDMA copies data into VRAM in
// 16-byte blocks. Bit 7 of HDMA5 picks HBlank mode
// (one block per HBlank) and bits 0-6 hold the
// number of blocks minus one.
static constexpr uint16_t HDMA_BLOCK_SIZE          = 0x10;
static constexpr uint8_t HDMA_HBLANK_MODE_BIT       = 7;
static constexpr uint8_t HDMA_LENGTH_MASK          = 0x7F;
static constexpr uint8_t HDMA_ADDRESS_LOW_MASK      = 0xF0; // Addresses are 16-byte aligned
static constexpr uint8_t HDMA_DESTINATION_HIGH_MASK = 0x1F; // Destination stays inside VRAM
static constexpr uint32_t HDMA_BLOCK_CYCLES        = 8;    // Machine cycles per block at normal speed

class Emulator;

class MMU
{
    private:
        Emulator &emulator;
        std::vector<uint8_t> workRAM;
        std::vector<uint8_t> oamRAM;
        std::vector<uint8_t> highRAM;
        uint8_t workRAMBank = 1;
        std::vector<uint8_t> cgbBootROM;
        uint16_t hdmaSource = 0;
        uint16_t hdmaDestination = VRAM_ADDRESS_START;
        uint8_t hdmaBlocksRemaining = 0;
        bool hdmaHBlank = false;
        ByteRegister disableBootROMSwitch;

        uint8_t readIO(const Address &address) const;
        void writeIO(const Address &address, const uint8_t value);
        void dmaTransfer(uint8_t value);
        bool isBootROMActive() const;
        void transferHDMABlock();
    public:
        MMU(Emulator &emulator);
        uint8_t read(const Address &address, bool bypassPPURestrictions = false) const;
        void write(const Address &address, const uint8_t value, bool bypassPPURestrictions = false);
        void hblankDMA();
        bool loadCGBBootROM(const std::vector<uint8_t> &rom);
        bool hasCGBBootROM() const { return !cgbBootROM.empty(); }
};
