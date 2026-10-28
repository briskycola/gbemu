#pragma once

#include "CartridgeMetadata.hpp"
#include "Address.hpp"
#include "Register.hpp"
#include <vector>
#include <memory>

// This file will be used to get
// the actual data from the cartridge,
// not the metadata.
//
// Not all Game Boy cartridges are
// the same. Some have Memory Bank
// Controllers (MBCs)
//
// Since the Game Boy had a very
// limited amount of RAM, some games
// took advantage of MBCs to expand
// the Game Boy's available RAM.
//
// The Game Boy will use bank switching
// to switch between main memory and
// the MBC.
//
// You can sort of think of it as
// swapping/paging in an OS,
// but more basic.
//
// https://gbdev.io/pandocs/MBCs.html

// ROM is mapped as two 16 KB windows: bank 0 is fixed at
// 0x0000-0x3FFF, and 0x4000-0x7FFF holds a switchable bank
static constexpr uint32_t ROM_BANK_SIZE            = 0x4000;
static constexpr uint16_t ROM_BANK0_END            = 0x3FFF;
static constexpr uint16_t SWITCHABLE_ROM_BANK_START = 0x4000;
static constexpr uint16_t SWITCHABLE_ROM_BANK_END   = 0x7FFF;

// Cartridge RAM is mapped in 8 KB banks at 0xA000-0xBFFF
static constexpr uint32_t RAM_BANK_SIZE = 0x2000;

// The game controls the MBC by writing to ROM addresses.
// MBC1, MBC3, and MBC5 share these ranges.
static constexpr uint16_t RAM_ENABLE_START         = 0x0000;
static constexpr uint16_t RAM_ENABLE_END           = 0x1FFF;
static constexpr uint16_t ROM_BANK_SELECT_START     = 0x2000;
static constexpr uint16_t ROM_BANK_SELECT_END       = 0x3FFF;
static constexpr uint16_t RAM_BANK_SELECT_START     = 0x4000;
static constexpr uint16_t RAM_BANK_SELECT_END       = 0x5FFF;
static constexpr uint16_t BANKING_MODE_SELECT_START = 0x6000;
static constexpr uint16_t BANKING_MODE_SELECT_END   = 0x7FFF;

// Writing 0xA to the low nibble of the RAM enable
// range enables cartridge RAM; anything else disables it
static constexpr uint8_t RAM_ENABLE_MASK  = 0x0F;
static constexpr uint8_t RAM_ENABLE_VALUE = 0x0A;

// MBC1 has a 5-bit ROM bank register and a 2-bit register that
// supplies either bits 5-6 of the ROM bank or the RAM bank
static constexpr uint8_t MBC1_ROM_BANK_LOW_MASK   = 0x1F;
static constexpr uint8_t MBC1_ROM_BANK_HIGH_MASK  = 0x60;
static constexpr uint8_t MBC1_ROM_BANK_HIGH_SHIFT = 5;
static constexpr uint8_t MBC1_TWO_BIT_MASK       = 0x03;

// MBC2 has 512 4-bit RAM cells built in. Bit 8 of the write
// address picks between RAM enable (0) and ROM bank select (1).
static constexpr uint16_t MBC2_RAM_SIZE           = 0x200;
static constexpr uint16_t MBC2_REGISTER_SELECT_BIT = 0x0100;
static constexpr uint8_t MBC2_NIBBLE_MASK         = 0x0F;
static constexpr uint8_t MBC2_UNUSED_BITS         = 0xF0; // The upper nibble of RAM reads as 1s

// MBC3 has a 7-bit ROM bank. Writing 0x00-0x03 to the RAM bank
// range selects a RAM bank, and 0x08-0x0C maps a real-time
// clock register (RTC) in its place.
static constexpr uint8_t MBC3_ROM_BANK_MASK = 0x7F;
static constexpr uint8_t MBC3_MAX_RAM_BANK  = 0x03;
static constexpr uint8_t MBC3_RTC_START    = 0x08;
static constexpr uint8_t MBC3_RTC_END      = 0x0C;

// MBC5 has a 9-bit ROM bank: the low 8 bits are written to
// 0x2000-0x2FFF and bit 8 to 0x3000-0x3FFF. The RAM bank is 4 bits.
static constexpr uint16_t MBC5_ROM_BANK_LOW_END    = 0x2FFF;
static constexpr uint16_t MBC5_ROM_BANK_HIGH_START = 0x3000;
static constexpr uint16_t MBC5_ROM_BANK_HIGH_BIT   = 0x100;
static constexpr uint8_t MBC5_RAM_BANK_MASK       = 0x0F;

class Cartridge
{
    protected:
        std::vector<uint8_t> rom;
        std::vector<uint8_t> ram;
        std::unique_ptr<CartridgeInfo> cartridgeInfo;

    public:
        Cartridge(std::vector<uint8_t> rom, const std::vector<uint8_t> &ram,
                  std::unique_ptr<CartridgeInfo> cartridgeInfo);
        virtual ~Cartridge() = default;

        virtual uint8_t read(const Address &address) const = 0;
        virtual void write(const Address &address, uint8_t value) = 0;
        virtual const std::vector<uint8_t>& getCartridgeRAM() const;
        bool supportsCGB() const;
        bool isCGBOnly() const;
        bool supportsSGB() const;
};

class MBC2 : public Cartridge
{
    private:
        uint8_t romBank = 1;
        bool ramEnabled = false;
        std::vector<uint8_t> nibbleRAM;

    public:
        MBC2(std::vector<uint8_t> rom, const std::vector<uint8_t> &ram,
             std::unique_ptr<CartridgeInfo> cartridgeInfo);
        uint8_t read(const Address &address) const override;
        void write(const Address &address, uint8_t value) override;
        const std::vector<uint8_t>& getCartridgeRAM() const override;
};

// The ram parameter is optional since not all
// Game Boy cartidges have a MBC.
std::shared_ptr<Cartridge> getCartridge(const std::vector<uint8_t> &rom,
                                        const std::vector<uint8_t> &ram = {});

class NoMBC : public Cartridge
{
    public:
        NoMBC(std::vector<uint8_t> rom, const std::vector<uint8_t> &ram,
              std::unique_ptr<CartridgeInfo> cartridgeInfo);

        uint8_t read(const Address &address) const override;
        void write(const Address &address, uint8_t value) override;
};

class MBC1 : public Cartridge
{
    private:
        WordRegister romBank;
        WordRegister ramBank;

        bool isRAMEnabled = false;
        bool bankingMode = false;

    public:
        MBC1(std::vector<uint8_t> rom, const std::vector<uint8_t> &ram,
             const std::unique_ptr<CartridgeInfo> cartridgeInfo);

        uint8_t read(const Address &address) const override;
        void write(const Address &address, uint8_t value) override;
};

class MBC3 : public Cartridge
{
    private:
        WordRegister romBank;
        WordRegister ramBank;

        bool isRAMEnabled = false;
        //bool romBankingMode = true;
        bool isRAMOverRTC = true;

    public:
        MBC3(std::vector<uint8_t> rom, const std::vector<uint8_t> &ram,
             const std::unique_ptr<CartridgeInfo> cartridgeInfo);

        uint8_t read(const Address &address) const override;
        void write(const Address &address, uint8_t value) override;
};

class MBC5 : public Cartridge
{
    private:
        uint16_t romBank = 1;
        uint8_t ramBank = 0;
        bool ramEnabled = false;

    public:
        MBC5(std::vector<uint8_t> rom, const std::vector<uint8_t> &ram,
             std::unique_ptr<CartridgeInfo> cartridgeInfo);
        uint8_t read(const Address &address) const override;
        void write(const Address &address, uint8_t value) override;
};
