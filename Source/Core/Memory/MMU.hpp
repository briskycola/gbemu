#pragma once

#include "Core/Common/Address.hpp"
#include "Core/Common/Register.hpp"
#include <array>
#include <vector>

// https://gbdev.io/pandocs/Memory_Map.html

// Constants to represent start and end addresses
// for each section of memory.
namespace mmu
{
    static constexpr uint16_t CARTRIDGE_ROM_ADDRESS_START = 0x0000;
    static constexpr uint16_t CARTRIDGE_ROM_ADDRESS_END   = 0x7FFF;
    static constexpr uint16_t VRAM_ADDRESS_START          = 0x8000;
    static constexpr uint16_t VRAM_ADDRESS_END            = 0x9FFF;
    static constexpr uint16_t CARTRIDGE_RAM_ADDRESS_START = 0xA000;
    static constexpr uint16_t CARTRIDGE_RAM_ADDRESS_END   = 0xBFFF;
    static constexpr uint16_t WRAM_ADDRESS_START          = 0xC000;
    static constexpr uint16_t WRAM_ADDRESS_END            = 0xDFFF;
    static constexpr uint16_t ECHO_RAM_ADDRESS_START      = 0xE000;
    static constexpr uint16_t ECHO_RAM_ADDRESS_END        = 0xFDFF;
    static constexpr uint16_t OAM_ADDRESS_START           = 0xFE00;
    static constexpr uint16_t OAM_ADDRESS_END             = 0xFE9F;
    static constexpr uint16_t UNUSABLE_ADDRESS_START      = 0xFEA0;
    static constexpr uint16_t UNUSABLE_ADDRESS_END        = 0xFEFF;
    static constexpr uint16_t IO_ADDRESS_START            = 0xFF00;
    static constexpr uint16_t IO_ADDRESS_END              = 0xFF7F;
    static constexpr uint16_t HRAM_ADDRESS_START          = 0xFF80;
    static constexpr uint16_t HRAM_ADDRESS_END            = 0xFFFE;
    static constexpr uint16_t INTERRUPT_ENABLE_REGISTER   = 0xFFFF;
    
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
    static constexpr uint16_t DMG_BOOT_ROM_SIZE              = 0x100;
    static constexpr uint16_t CGB_BOOT_ROM_SECOND_PART_START = 0x200;
    static constexpr uint16_t CGB_BOOT_ROM_SIZE              = 0x900;
    
    // OAM DMA copies 160 bytes from page (value * 0x100)
    // into OAM, one byte per machine cycle, after a one
    // cycle startup delay. The CPU keeps running, but while
    // the transfer is active it cannot reach OAM or the bus
    // the DMA is reading from (HRAM and I/O stay usable).
    static constexpr uint16_t DMA_SOURCE_PAGE_SHIFT = 0x100;
    static constexpr uint16_t DMA_TRANSFER_LENGTH   = 0xA0;
    static constexpr uint8_t OAM_DMA_START_DELAY    = 1;
    
    // Game Boy Color HDMA copies data into VRAM in
    // 16-byte blocks. Bit 7 of HDMA5 picks HBlank mode
    // (one block per HBlank) and bits 0-6 hold the
    // number of blocks minus one.
    static constexpr uint16_t HDMA_BLOCK_SIZE           = 0x10;
    static constexpr uint8_t HDMA_HBLANK_MODE_BIT       = 7;
    static constexpr uint8_t HDMA_LENGTH_MASK           = 0x7F;
    static constexpr uint8_t HDMA_ADDRESS_LOW_MASK      = 0xF0; // Addresses are 16-byte aligned
    static constexpr uint8_t HDMA_DESTINATION_HIGH_MASK = 0x1F; // Destination stays inside VRAM
    static constexpr uint32_t HDMA_BLOCK_CYCLES         = 8;    // Machine cycles per block at normal speed

    // While scanning OAM (mode 2) the PPU reads it as 20 rows
    // of 8 bytes, one row per machine cycle
    static constexpr int OAM_ROW_COUNT           = 20;
    static constexpr int OAM_ROW_SIZE            = 8;
    static constexpr int OAM_SCAN_CLOCKS_PER_ROW = 4;
}

// https://gbdev.io/pandocs/OAM_Corruption_Bug.html
//
// On the DMG, putting an address in $FE00-$FEFF on the bus or through the
// CPU's 16-bit increment/decrement unit while the PPU scans OAM (mode 2)
// corrupts the OAM row the PPU is reading. The pattern depends on what the
// CPU did during that machine cycle.
enum class OAMCorruption : uint8_t
{
    Read,          // A read
    Write,         // A write, or a 16-bit increment/decrement on its own
    ReadIncrease,  // A read and a 16-bit increment/decrement in the same cycle
};

class Emulator;

class MMU
{
    private:
        Emulator &emulator;
        std::vector<uint8_t> workRAM;
        std::vector<uint8_t> oamRAM;
        std::vector<uint8_t> highRAM;
        uint8_t workRAMBank = 1;
        std::array<uint8_t, 3> undocumentedRegisters{}; // CGB FF72-FF74
        uint8_t undocumentedFF75 = 0;          // CGB FF75, bits 4-6
        uint8_t key0 = 0;                      // CGB KEY0, written by the boot ROM
        uint8_t infraredPort = 0;              // CGB RP
        std::vector<uint8_t> bootROM; // 256 bytes (Game Boy) or 2304 bytes (Game Boy Color)
        uint16_t hdmaSource = 0;
        uint16_t hdmaDestination = mmu::VRAM_ADDRESS_START;
        uint8_t hdmaBlocksRemaining = 0;
        bool hdmaHBlankMode = false;
        ByteRegister disableBootROMSwitch;

        // OAM DMA state
        enum class Bus { None, External, WorkRAM, Video };
        uint8_t oamDMARegister = 0xFF;     // Last value written to 0xFF46
        uint16_t oamDMASource = 0;         // Source of the running transfer
        uint8_t oamDMAIndex = 0;           // Next byte to copy
        bool oamDMAActive = false;         // Transfer in progress (bus is locked)
        uint8_t oamDMALastByte = 0xFF;     // Byte currently on the DMA's bus
        uint16_t oamDMAPendingSource = 0;  // Source of a requested (re)start
        bool oamDMAStartRequested = false; // 0xFF46 written during this CPU step
        uint8_t oamDMAStartDelay = 0;      // Cycles until the pending transfer starts

        uint8_t readIO(const Address &address) const;
        void writeIO(const Address &address, const uint8_t value);
        void requestOAMDMA(uint8_t value);
        void copyOAMDMAByte();
        Bus busOf(uint16_t address) const;
        bool oamDMABlocksCPU(uint16_t address) const;
        bool isBootROMActive() const;
        void transferHDMABlock();
        uint16_t oamWord(int row, int word) const;
        void setOAMWord(int row, int word, uint16_t value);
        void copyOAMRow(int from, int to);
    public:
        // Applies the DMG OAM corruption bug for an address the CPU put on
        // the bus or through its increment/decrement unit this machine cycle
        void corruptOAM(uint16_t address, OAMCorruption kind);
        MMU(Emulator &emulator);
        uint8_t read(const Address &address, bool bypassPPURestrictions = false) const;
        void write(const Address &address, const uint8_t value, bool bypassPPURestrictions = false);
        void tickOAMDMA(uint32_t mCycles);
        uint32_t hblankDMA(); // Returns CPU M-cycles consumed by the DMA transfer
        void loadBootROM(const std::vector<uint8_t> &rom);
};
