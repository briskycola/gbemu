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
namespace cartridge
{
    static constexpr uint32_t ROM_BANK_SIZE             = 0x4000;
    static constexpr uint16_t ROM_BANK0_END             = 0x3FFF;
    static constexpr uint16_t SWITCHABLE_ROM_BANK_START = 0x4000;
    static constexpr uint16_t SWITCHABLE_ROM_BANK_END   = 0x7FFF;
    
    // Cartridge RAM is mapped in 8 KB banks at 0xA000-0xBFFF
    static constexpr uint32_t RAM_BANK_SIZE = 0x2000;
    
    // The game controls the MBC by writing to ROM addresses.
    // MBC1, MBC3, and MBC5 share these ranges.
    static constexpr uint16_t RAM_ENABLE_START          = 0x0000;
    static constexpr uint16_t RAM_ENABLE_END            = 0x1FFF;
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
    static constexpr uint8_t MBC1_ROM_BANK_HIGH_SHIFT = 5;
    static constexpr uint8_t MBC1_TWO_BIT_MASK        = 0x03;
    static constexpr uint8_t MBC1_BANKING_MODE_BIT    = 0x01;

    // MBC1M multicarts are 1 MiB, use only 4 bits of the 5-bit register and
    // are recognised by a second Nintendo logo in bank $10
    // https://gbdev.io/pandocs/MBC1.html#mbc1m-1-mib-multi-game-compilation-carts
    static constexpr uint32_t MBC1M_ROM_SIZE           = 0x100000;
    static constexpr uint8_t MBC1M_ROM_BANK_LOW_MASK   = 0x0F;
    static constexpr uint8_t MBC1M_ROM_BANK_HIGH_SHIFT = 4;
    static constexpr uint8_t MBC1M_LOGO_BANK           = 0x10;
    static constexpr uint16_t NINTENDO_LOGO_LENGTH     = 0x30;
    
    // MBC2 has 512 4-bit RAM cells built in. Bit 8 of the write
    // address picks between RAM enable (0) and ROM bank select (1).
    static constexpr uint16_t MBC2_RAM_SIZE            = 0x200;
    static constexpr uint16_t MBC2_REGISTER_SELECT_BIT = 0x0100;
    static constexpr uint8_t MBC2_NIBBLE_MASK          = 0x0F;
    static constexpr uint8_t MBC2_UNUSED_BITS          = 0xF0; // The upper nibble of RAM reads as 1s
    
    // MBC3 has a 7-bit ROM bank (8 bits on the MBC30, which has more than
    // 2 MiB of ROM). Writing 0x00-0x07 to the RAM bank range selects a RAM
    // bank, and 0x08-0x0C maps a real-time clock (RTC) register instead.
    // https://gbdev.io/pandocs/MBC3.html
    static constexpr uint8_t MBC3_ROM_BANK_MASK    = 0x7F;
    static constexpr uint8_t MBC30_ROM_BANK_MASK   = 0xFF;
    static constexpr uint32_t MBC30_MIN_ROM_SIZE   = 0x200001;
    static constexpr uint8_t MBC3_MAX_RAM_BANK     = 0x07;
    static constexpr uint8_t MBC3_RTC_SECONDS      = 0x08;
    static constexpr uint8_t MBC3_RTC_MINUTES      = 0x09;
    static constexpr uint8_t MBC3_RTC_HOURS        = 0x0A;
    static constexpr uint8_t MBC3_RTC_DAYS_LOW     = 0x0B;
    static constexpr uint8_t MBC3_RTC_DAYS_HIGH    = 0x0C;

    // The clock runs from a 32.768 kHz crystal: one second per this many
    // clocks of the 4.194304 MHz system clock
    static constexpr uint32_t MBC3_CLOCKS_PER_SECOND = 4194304;

    // Each counter's bits, and the value where it wraps to 0 and carries.
    // An out-of-range value counts up to the top of its bits and wraps to 0
    // without carrying.
    static constexpr uint8_t MBC3_SECONDS_MASK   = 0x3F;
    static constexpr uint8_t MBC3_MINUTES_MASK   = 0x3F;
    static constexpr uint8_t MBC3_HOURS_MASK     = 0x1F;
    static constexpr uint8_t MBC3_SECONDS_WRAP   = 60;
    static constexpr uint8_t MBC3_MINUTES_WRAP   = 60;
    static constexpr uint8_t MBC3_HOURS_WRAP     = 24;
    static constexpr uint8_t MBC3_DAY_HIGH_BIT   = 0x01; // DH bit 0: bit 8 of the day counter
    static constexpr uint8_t MBC3_HALT_BIT       = 0x40; // DH bit 6: clock stopped
    static constexpr uint8_t MBC3_DAY_CARRY_BIT  = 0x80; // DH bit 7: day counter overflowed
    static constexpr uint8_t MBC3_DAYS_HIGH_MASK = MBC3_DAY_HIGH_BIT | MBC3_HALT_BIT | MBC3_DAY_CARRY_BIT;

    // Writing $00 then $01 to 6000-7FFF latches the clock
    static constexpr uint8_t MBC3_LATCH_FIRST  = 0x00;
    static constexpr uint8_t MBC3_LATCH_SECOND = 0x01;

    // Saves keep the clock after the RAM in the format BGB and VBA-M use:
    // the five counters, the five latched counters (each a little-endian
    // 32-bit value), then the time it was saved as a 64-bit UNIX time.
    // Older files have a 32-bit time.
    static constexpr size_t RTC_SAVE_REGISTER_COUNT = 10;
    static constexpr size_t RTC_SAVE_SIZE           = RTC_SAVE_REGISTER_COUNT * 4 + 8;
    static constexpr size_t RTC_SAVE_SIZE_OLD       = RTC_SAVE_REGISTER_COUNT * 4 + 4;

    // MBC5 rumble carts use RAM bank bit 3 for the motor
    // https://gbdev.io/pandocs/MBC5.html#4000-5fff---ram-bank-number-write-only
    static constexpr uint8_t MBC5_RUMBLE_BIT       = 0x08;
    static constexpr uint8_t MBC5_RUMBLE_BANK_MASK = 0x07;
    
    // MBC5 has a 9-bit ROM bank: the low 8 bits are written to
    // 0x2000-0x2FFF and bit 8 to 0x3000-0x3FFF. The RAM bank is 4 bits.
    static constexpr uint16_t MBC5_ROM_BANK_LOW_END    = 0x2FFF;
    static constexpr uint16_t MBC5_ROM_BANK_HIGH_START = 0x3000;
    static constexpr uint16_t MBC5_ROM_BANK_HIGH_BIT   = 0x100;
    static constexpr uint8_t MBC5_RAM_BANK_MASK        = 0x0F;
}

class Cartridge
{
    protected:
        std::vector<uint8_t> rom;
        std::vector<uint8_t> ram;
        std::unique_ptr<CartridgeInfo> cartridgeInfo;

        // Bytes a save file holds after the RAM, e.g. the MBC3 clock
        std::vector<uint8_t> saveFooter;

        // What a battery-backed save holds besides the RAM
        virtual std::vector<uint8_t> saveExtra() const { return {}; }

    public:
        Cartridge(std::vector<uint8_t> rom, const std::vector<uint8_t> &save,
                  std::unique_ptr<CartridgeInfo> cartridgeInfo);
        virtual ~Cartridge() = default;

        virtual uint8_t read(const Address &address) const = 0;
        virtual void write(const Address &address, uint8_t value) = 0;

        // Runs hardware on the cartridge, like the MBC3 clock, for some
        // clocks of the 4.194304 MHz system clock (real time, not CPU speed)
        virtual void tick(uint32_t clocks) { (void)clocks; }

        // What a battery keeps when the Game Boy is off: the cartridge RAM
        // and anything else battery-backed. Empty without a battery.
        std::vector<uint8_t> saveData() const;
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

// https://gbdev.io/pandocs/MBC1.html
class MBC1 : public Cartridge
{
    private:
        uint8_t romBankRegister = 0; // 5-bit register at 2000-3FFF
        uint8_t bank2Register = 0;   // 2-bit register at 4000-5FFF
        bool ramEnabled = false;
        bool bankingMode = false;    // Mode 1 also applies bank2Register to 0000-3FFF and RAM

        // MBC1M multi-game carts wire the 2-bit register to bank bits 4-5
        // and ignore the top bit of the 5-bit register
        bool multicart = false;

        uint32_t romOffset(uint32_t bank) const;
        uint32_t ramIndex(const Address &address) const;

    public:
        MBC1(std::vector<uint8_t> rom, const std::vector<uint8_t> &ram,
             const std::unique_ptr<CartridgeInfo> cartridgeInfo);

        uint8_t read(const Address &address) const override;
        void write(const Address &address, uint8_t value) override;
};

// https://gbdev.io/pandocs/MBC3.html
class MBC3 : public Cartridge
{
    private:
        // The real-time clock's counters: seconds, minutes, hours, the low
        // 8 bits of the day counter, and DH (day bit 8, halt, day carry)
        struct Clock
        {
            uint8_t seconds = 0;
            uint8_t minutes = 0;
            uint8_t hours = 0;
            uint8_t daysLow = 0;
            uint8_t daysHigh = 0;
        };

        uint8_t romBank = 1;
        uint8_t bankSelect = 0;  // RAM bank 00-07, or RTC register 08-0C
        bool ramEnabled = false;
        Clock clock;             // Counting registers
        Clock latched;           // What reads see, copied from clock when latched
        uint8_t lastLatchWrite = 0xFF;
        uint32_t clocksIntoSecond = 0;

        void advanceSecond();
        void loadClock(const std::vector<uint8_t> &footer);
        static uint8_t clockRegisterValue(const Clock &registers, uint8_t select);
        static void setClockRegister(Clock &registers, uint8_t select, uint8_t value);

    protected:
        std::vector<uint8_t> saveExtra() const override;

    public:
        MBC3(std::vector<uint8_t> rom, const std::vector<uint8_t> &ram,
             const std::unique_ptr<CartridgeInfo> cartridgeInfo);

        uint8_t read(const Address &address) const override;
        void write(const Address &address, uint8_t value) override;
        void tick(uint32_t clocks) override;
};

// https://gbdev.io/pandocs/MBC5.html
class MBC5 : public Cartridge
{
    private:
        uint16_t romBank = 1;
        uint8_t ramBank = 0;
        bool ramEnabled = false;
        bool rumbleOn = false; // Rumble carts drive the motor with RAM bank bit 3

    public:
        MBC5(std::vector<uint8_t> rom, const std::vector<uint8_t> &ram,
             std::unique_ptr<CartridgeInfo> cartridgeInfo);
        uint8_t read(const Address &address) const override;
        void write(const Address &address, uint8_t value) override;
};
