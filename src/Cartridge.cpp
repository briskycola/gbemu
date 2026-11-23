#include "Cartridge.hpp"
#include "Address.hpp"
#include "CartridgeMetadata.hpp"
#include "File.hpp"
#include "MMU.hpp"
#include "IORegisters.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>


Cartridge::Cartridge(std::vector<uint8_t> rom, const std::vector<uint8_t> &save,
                     std::unique_ptr<CartridgeInfo> cartridgeInfo) :
    rom(std::move(rom)),
    cartridgeInfo(std::move(cartridgeInfo))
{
    const uint32_t ramSize = getActualRamSize(this->cartridgeInfo->ramSize);
    if (save.empty())
    {
        ram = std::vector<uint8_t>(ramSize, 0);
        return;
    }

    // A save is the RAM, followed by the clock on carts that have one
    const bool sizeMatches = save.size() == ramSize ||
        (this->cartridgeInfo->hasTimer && (save.size() == ramSize + cartridge::RTC_SAVE_SIZE ||
                                           save.size() == ramSize + cartridge::RTC_SAVE_SIZE_OLD));
    if (!sizeMatches)
    {
        throw std::runtime_error("Invalid save file: expected " + std::to_string(ramSize)
            + " bytes of RAM, got " + std::to_string(save.size()) + " bytes");
    }
    ram.assign(save.begin(), save.begin() + ramSize);
    saveFooter.assign(save.begin() + ramSize, save.end());
}

std::vector<uint8_t> Cartridge::saveData() const
{
    if (!cartridgeInfo->hasBattery) return {};
    std::vector<uint8_t> data = getCartridgeRAM();
    const std::vector<uint8_t> extra = saveExtra();
    data.insert(data.end(), extra.begin(), extra.end());
    return data;
}

NoMBC::NoMBC(std::vector<uint8_t> rom, const std::vector<uint8_t> &ram,
             std::unique_ptr<CartridgeInfo> cartridgeInfo) :
    Cartridge(std::move(rom), ram, std::move(cartridgeInfo))
{}

MBC1::MBC1(std::vector<uint8_t> rom, const std::vector<uint8_t> &ram,
             std::unique_ptr<CartridgeInfo> cartridgeInfo) :
    Cartridge(std::move(rom), ram, std::move(cartridgeInfo))
{
    // A 1 MiB MBC1 cart with a Nintendo logo in bank $10 as well is an MBC1M
    // multicart: each game has its own header there
    // https://gbdev.io/pandocs/MBC1.html#mbc1m-1-mib-multi-game-compilation-carts
    const uint32_t secondLogo = cartridge::MBC1M_LOGO_BANK * cartridge::ROM_BANK_SIZE + cartridge::NINTENDO_LOGO_START_ADDRESS;
    multicart = this->rom.size() == cartridge::MBC1M_ROM_SIZE &&
        std::equal(this->rom.begin() + cartridge::NINTENDO_LOGO_START_ADDRESS,
                   this->rom.begin() + cartridge::NINTENDO_LOGO_START_ADDRESS + cartridge::NINTENDO_LOGO_LENGTH,
                   this->rom.begin() + secondLogo);
}

MBC2::MBC2(std::vector<uint8_t> rom, const std::vector<uint8_t> &save,
           std::unique_ptr<CartridgeInfo> cartridgeInfo) :
    Cartridge(std::move(rom), {}, std::move(cartridgeInfo)), nibbleRAM(cartridge::MBC2_RAM_SIZE, cartridge::MBC2_NIBBLE_MASK)
{
    if (!save.empty())
    {
        if (save.size() != nibbleRAM.size()) throw std::runtime_error("Invalid MBC2 save size");
        for (size_t i = 0; i < nibbleRAM.size(); ++i) nibbleRAM[i] = save[i] & cartridge::MBC2_NIBBLE_MASK;
    }
}

MBC3::MBC3(std::vector<uint8_t> rom, const std::vector<uint8_t> &ram,
             std::unique_ptr<CartridgeInfo> cartridgeInfo) :
    Cartridge(std::move(rom), ram, std::move(cartridgeInfo))
{
    if (this->cartridgeInfo->hasTimer) loadClock(saveFooter);
}

MBC5::MBC5(std::vector<uint8_t> rom, const std::vector<uint8_t> &ram,
           std::unique_ptr<CartridgeInfo> cartridgeInfo) :
    Cartridge(std::move(rom), ram, std::move(cartridgeInfo)) {}

std::shared_ptr<Cartridge> getCartridge(const std::vector<uint8_t> &rom,
                                        const std::vector<uint8_t> &ram)
{
    std::unique_ptr<CartridgeInfo> cartridgeInfo;
    cartridgeInfo = getInfo(rom);

    switch (cartridgeInfo->type)
    {
        case CartridgeType::ROMOnly:
            return std::make_shared<NoMBC>(rom, ram, std::move(cartridgeInfo));
        case CartridgeType::MBC1:
            return std::make_shared<MBC1>(rom, ram, std::move(cartridgeInfo));
        case CartridgeType::MBC2:
            return std::make_shared<MBC2>(rom, ram, std::move(cartridgeInfo));
        case CartridgeType::MBC3:
            return std::make_shared<MBC3>(rom, ram, std::move(cartridgeInfo));
        case CartridgeType::MBC4:
            throw std::runtime_error("MBC4 is not implemented");
        case CartridgeType::MBC5:
            return std::make_shared<MBC5>(rom, ram, std::move(cartridgeInfo));
        case CartridgeType::NotImplemented:
            throw std::runtime_error("Not implemented");
        default:
            throw std::runtime_error("Unknown cartridge type");
    }
    return nullptr;
}

const std::vector<uint8_t>& Cartridge::getCartridgeRAM() const
{
    return ram;
}

bool Cartridge::supportsCGB() const { return cartridgeInfo->supportsCGB; }
bool Cartridge::isCGBOnly() const { return cartridgeInfo->cgbOnly; }
bool Cartridge::supportsSGB() const { return cartridgeInfo->supportsSGB; }

uint8_t NoMBC::read(const Address &address) const
{
    if (address.inRange(mmu::CARTRIDGE_ROM_ADDRESS_START, mmu::CARTRIDGE_ROM_ADDRESS_END))
    {
        return rom.at(address.getValue());
    }

    else if (address.inRange(mmu::CARTRIDGE_RAM_ADDRESS_START, mmu::CARTRIDGE_RAM_ADDRESS_END))
    {
        return ram.empty() ? io::UNMAPPED_READ_VALUE : ram.at((address.getValue() - mmu::CARTRIDGE_RAM_ADDRESS_START) % ram.size());
    }
    else
    {
        throw std::runtime_error("Invalid address for NoMBC cartridge");
    }

    return io::UNMAPPED_READ_VALUE;
}

void NoMBC::write(const Address &address, uint8_t value)
{
    if (address.inRange(mmu::CARTRIDGE_RAM_ADDRESS_START, mmu::CARTRIDGE_RAM_ADDRESS_END) && !ram.empty())
        ram.at((address.getValue() - mmu::CARTRIDGE_RAM_ADDRESS_START) % ram.size()) = value;
}

uint8_t MBC2::read(const Address &address) const
{
    if (address.inRange(mmu::CARTRIDGE_ROM_ADDRESS_START, cartridge::ROM_BANK0_END)) return rom.at(address.getValue());
    if (address.inRange(cartridge::SWITCHABLE_ROM_BANK_START, cartridge::SWITCHABLE_ROM_BANK_END))
    {
        const size_t banks = rom.size() / cartridge::ROM_BANK_SIZE;
        return rom.at((banks ? romBank % banks : 0) * cartridge::ROM_BANK_SIZE + address.getValue() - cartridge::SWITCHABLE_ROM_BANK_START);
    }
    if (address.inRange(mmu::CARTRIDGE_RAM_ADDRESS_START, mmu::CARTRIDGE_RAM_ADDRESS_END) && ramEnabled)
        return cartridge::MBC2_UNUSED_BITS | nibbleRAM[(address.getValue() - mmu::CARTRIDGE_RAM_ADDRESS_START) % cartridge::MBC2_RAM_SIZE];
    return io::UNMAPPED_READ_VALUE;
}

void MBC2::write(const Address &address, uint8_t value)
{
    if (address.inRange(mmu::CARTRIDGE_ROM_ADDRESS_START, cartridge::ROM_BANK0_END))
    {
        if ((address.getValue() & cartridge::MBC2_REGISTER_SELECT_BIT) == 0) ramEnabled = (value & cartridge::RAM_ENABLE_MASK) == cartridge::RAM_ENABLE_VALUE;
        else { romBank = value & cartridge::MBC2_NIBBLE_MASK; if (romBank == 0) romBank = 1; }
    }
    else if (address.inRange(mmu::CARTRIDGE_RAM_ADDRESS_START, mmu::CARTRIDGE_RAM_ADDRESS_END) && ramEnabled)
        nibbleRAM[(address.getValue() - mmu::CARTRIDGE_RAM_ADDRESS_START) % cartridge::MBC2_RAM_SIZE] = value & cartridge::MBC2_NIBBLE_MASK;
}

const std::vector<uint8_t>& MBC2::getCartridgeRAM() const { return nibbleRAM; }

// Bank numbers larger than the ROM are masked to the bits it needs
// https://gbdev.io/pandocs/MBC1.html#20003fff--rom-bank-number-write-only
uint32_t MBC1::romOffset(uint32_t bank) const
{
    const uint32_t bankCount = static_cast<uint32_t>(rom.size() / cartridge::ROM_BANK_SIZE);
    return bankCount ? (bank % bankCount) * cartridge::ROM_BANK_SIZE : 0;
}

// In mode 1 the 2-bit register selects the RAM bank; in mode 0 it is bank 0
// https://gbdev.io/pandocs/MBC1.html#a000bfff--ram-bank-0003-if-any
uint32_t MBC1::ramIndex(const Address &address) const
{
    const uint32_t bank = bankingMode ? bank2Register : 0;
    return (bank * cartridge::RAM_BANK_SIZE + address.getValue() - mmu::CARTRIDGE_RAM_ADDRESS_START) % ram.size();
}

uint8_t MBC1::read(const Address &address) const
{
    const uint8_t highShift = multicart ? cartridge::MBC1M_ROM_BANK_HIGH_SHIFT : cartridge::MBC1_ROM_BANK_HIGH_SHIFT;

    // 0000-3FFF is bank 0, or in mode 1 the bank the 2-bit register selects
    // ($20/$40/$60, or $10/$20/$30 on a multicart)
    // https://gbdev.io/pandocs/MBC1.html#00003fff--rom-bank-x0-read-only
    if (address.inRange(mmu::CARTRIDGE_ROM_ADDRESS_START, cartridge::ROM_BANK0_END))
    {
        const uint32_t bank = bankingMode ? static_cast<uint32_t>(bank2Register) << highShift : 0;
        return rom.at(romOffset(bank) + address.getValue());
    }

    // 4000-7FFF is the 5-bit register's bank (0 counts as 1) plus the 2-bit
    // register above it. A multicart ignores the 5-bit register's top bit.
    // https://gbdev.io/pandocs/MBC1.html#40007fff--rom-bank-01-7f-read-only
    if (address.inRange(cartridge::SWITCHABLE_ROM_BANK_START, cartridge::SWITCHABLE_ROM_BANK_END))
    {
        const uint8_t low = romBankRegister ? romBankRegister : 1;
        const uint8_t lowMask = multicart ? cartridge::MBC1M_ROM_BANK_LOW_MASK : cartridge::MBC1_ROM_BANK_LOW_MASK;
        const uint32_t bank = (static_cast<uint32_t>(bank2Register) << highShift) | (low & lowMask);
        return rom.at(romOffset(bank) + address.getValue() - cartridge::SWITCHABLE_ROM_BANK_START);
    }

    // Disabled or missing RAM reads open bus ($FF)
    if (address.inRange(mmu::CARTRIDGE_RAM_ADDRESS_START, mmu::CARTRIDGE_RAM_ADDRESS_END))
    {
        if (!ramEnabled || ram.empty()) return io::UNMAPPED_READ_VALUE;
        return ram[ramIndex(address)];
    }
    throw std::runtime_error("Attempting to read from unmapped MBC1 address");
}

// https://gbdev.io/pandocs/MBC1.html#registers
void MBC1::write(const Address &address, uint8_t value)
{
    if (address.inRange(cartridge::RAM_ENABLE_START, cartridge::RAM_ENABLE_END))
        ramEnabled = (value & cartridge::RAM_ENABLE_MASK) == cartridge::RAM_ENABLE_VALUE;
    else if (address.inRange(cartridge::ROM_BANK_SELECT_START, cartridge::ROM_BANK_SELECT_END))
        romBankRegister = value & cartridge::MBC1_ROM_BANK_LOW_MASK;
    else if (address.inRange(cartridge::RAM_BANK_SELECT_START, cartridge::RAM_BANK_SELECT_END))
        bank2Register = value & cartridge::MBC1_TWO_BIT_MASK;
    else if (address.inRange(cartridge::BANKING_MODE_SELECT_START, cartridge::BANKING_MODE_SELECT_END))
        bankingMode = (value & cartridge::MBC1_BANKING_MODE_BIT) != 0;

    // Writes go nowhere when RAM is disabled or the cartridge has none
    else if (address.inRange(mmu::CARTRIDGE_RAM_ADDRESS_START, mmu::CARTRIDGE_RAM_ADDRESS_END) && ramEnabled && !ram.empty())
        ram[ramIndex(address)] = value;
}

// https://gbdev.io/pandocs/MBC3.html#memory
uint8_t MBC3::read(const Address &address) const
{
    if (address.inRange(mmu::CARTRIDGE_ROM_ADDRESS_START, cartridge::ROM_BANK0_END))
        return rom.at(address.getValue());

    if (address.inRange(cartridge::SWITCHABLE_ROM_BANK_START, cartridge::SWITCHABLE_ROM_BANK_END))
    {
        const uint32_t bankCount = static_cast<uint32_t>(rom.size() / cartridge::ROM_BANK_SIZE);
        const uint32_t bank = bankCount ? romBank % bankCount : 0;
        return rom.at(bank * cartridge::ROM_BANK_SIZE + address.getValue() - cartridge::SWITCHABLE_ROM_BANK_START);
    }

    // A000-BFFF is a RAM bank or one clock register. Reads of the clock see
    // the latched copy. Disabled RAM reads open bus ($FF).
    if (address.inRange(mmu::CARTRIDGE_RAM_ADDRESS_START, mmu::CARTRIDGE_RAM_ADDRESS_END))
    {
        if (!ramEnabled) return io::UNMAPPED_READ_VALUE;
        if (bankSelect >= cartridge::MBC3_RTC_SECONDS)
        {
            if (!cartridgeInfo->hasTimer || bankSelect > cartridge::MBC3_RTC_DAYS_HIGH) return io::UNMAPPED_READ_VALUE;
            return clockRegisterValue(latched, bankSelect);
        }
        if (ram.empty()) return io::UNMAPPED_READ_VALUE;
        return ram[(bankSelect * cartridge::RAM_BANK_SIZE + address.getValue() - mmu::CARTRIDGE_RAM_ADDRESS_START) % ram.size()];
    }
    throw std::runtime_error("Attempting to read from unmapped MBC3 address");
}

// https://gbdev.io/pandocs/MBC3.html#registers
void MBC3::write(const Address &address, uint8_t value)
{
    // RAM and clock enable
    if (address.inRange(cartridge::RAM_ENABLE_START, cartridge::RAM_ENABLE_END))
    {
        ramEnabled = (value & cartridge::RAM_ENABLE_MASK) == cartridge::RAM_ENABLE_VALUE;
    }

    // ROM bank: 7 bits (8 on the MBC30), and 0 selects bank 1
    else if (address.inRange(cartridge::ROM_BANK_SELECT_START, cartridge::ROM_BANK_SELECT_END))
    {
        const uint8_t mask = rom.size() >= cartridge::MBC30_MIN_ROM_SIZE ? cartridge::MBC30_ROM_BANK_MASK : cartridge::MBC3_ROM_BANK_MASK;
        romBank = value & mask;
        if (romBank == 0) romBank = 1;
    }

    // RAM bank 00-07 or clock register 08-0C
    else if (address.inRange(cartridge::RAM_BANK_SELECT_START, cartridge::RAM_BANK_SELECT_END))
    {
        bankSelect = value;
    }

    // Writing $00 then $01 copies the clock into the latched registers
    else if (address.inRange(cartridge::BANKING_MODE_SELECT_START, cartridge::BANKING_MODE_SELECT_END))
    {
        if (lastLatchWrite == cartridge::MBC3_LATCH_FIRST && value == cartridge::MBC3_LATCH_SECOND) latched = clock;
        lastLatchWrite = value;
    }

    else if (address.inRange(mmu::CARTRIDGE_RAM_ADDRESS_START, mmu::CARTRIDGE_RAM_ADDRESS_END) && ramEnabled)
    {
        if (bankSelect >= cartridge::MBC3_RTC_SECONDS)
        {
            if (!cartridgeInfo->hasTimer || bankSelect > cartridge::MBC3_RTC_DAYS_HIGH) return;

            // Writes set the counting register, and show in the latched copy
            // too. Writing the seconds also restarts the current second.
            setClockRegister(clock, bankSelect, value);
            setClockRegister(latched, bankSelect, value);
            if (bankSelect == cartridge::MBC3_RTC_SECONDS) clocksIntoSecond = 0;
            return;
        }
        if (ram.empty()) return;
        ram[(bankSelect * cartridge::RAM_BANK_SIZE + address.getValue() - mmu::CARTRIDGE_RAM_ADDRESS_START) % ram.size()] = value;
    }
}

// Clock registers keep only the bits they have
// https://gbdev.io/pandocs/MBC3.html#clock-counter-registers
uint8_t MBC3::clockRegisterValue(const Clock &registers, uint8_t select)
{
    switch (select)
    {
        case cartridge::MBC3_RTC_SECONDS: return registers.seconds;
        case cartridge::MBC3_RTC_MINUTES: return registers.minutes;
        case cartridge::MBC3_RTC_HOURS: return registers.hours;
        case cartridge::MBC3_RTC_DAYS_LOW: return registers.daysLow;
        default: return registers.daysHigh;
    }
}

void MBC3::setClockRegister(Clock &registers, uint8_t select, uint8_t value)
{
    switch (select)
    {
        case cartridge::MBC3_RTC_SECONDS: registers.seconds = value & cartridge::MBC3_SECONDS_MASK; break;
        case cartridge::MBC3_RTC_MINUTES: registers.minutes = value & cartridge::MBC3_MINUTES_MASK; break;
        case cartridge::MBC3_RTC_HOURS: registers.hours = value & cartridge::MBC3_HOURS_MASK; break;
        case cartridge::MBC3_RTC_DAYS_LOW: registers.daysLow = value; break;
        default: registers.daysHigh = value & cartridge::MBC3_DAYS_HIGH_MASK; break;
    }
}

// The clock counts while not halted, from the 32.768 kHz crystal
void MBC3::tick(uint32_t clocks)
{
    if (!cartridgeInfo->hasTimer || (clock.daysHigh & cartridge::MBC3_HALT_BIT)) return;
    clocksIntoSecond += clocks;
    while (clocksIntoSecond >= cartridge::MBC3_CLOCKS_PER_SECOND)
    {
        clocksIntoSecond -= cartridge::MBC3_CLOCKS_PER_SECOND;
        advanceSecond();
    }
}

// Counts one second, carrying into minutes, hours and the 9-bit day counter.
// The day counter's carry bit stays set once it overflows.
// https://gbdev.io/pandocs/MBC3.html#the-day-counter
void MBC3::advanceSecond()
{
    // Each counter wraps within its bits, carrying only when it reaches its limit
    const auto count = [](uint8_t &counter, uint8_t mask, uint8_t limit)
    {
        counter = static_cast<uint8_t>((counter + 1) & mask);
        if (counter != limit) return false;
        counter = 0;
        return true;
    };
    if (!count(clock.seconds, cartridge::MBC3_SECONDS_MASK, cartridge::MBC3_SECONDS_WRAP)) return;
    if (!count(clock.minutes, cartridge::MBC3_MINUTES_MASK, cartridge::MBC3_MINUTES_WRAP)) return;
    if (!count(clock.hours, cartridge::MBC3_HOURS_MASK, cartridge::MBC3_HOURS_WRAP)) return;
    if (++clock.daysLow != 0) return;
    if (clock.daysHigh & cartridge::MBC3_DAY_HIGH_BIT)
        clock.daysHigh = static_cast<uint8_t>((clock.daysHigh & ~cartridge::MBC3_DAY_HIGH_BIT) | cartridge::MBC3_DAY_CARRY_BIT);
    else
        clock.daysHigh |= cartridge::MBC3_DAY_HIGH_BIT;
}

namespace
{
    uint64_t readLittleEndian(const std::vector<uint8_t> &data, size_t offset, size_t bytes)
    {
        uint64_t value = 0;
        for (size_t i = 0; i < bytes; ++i) value |= static_cast<uint64_t>(data[offset + i]) << (8 * i);
        return value;
    }

    void writeLittleEndian(std::vector<uint8_t> &data, uint64_t value, size_t bytes)
    {
        for (size_t i = 0; i < bytes; ++i) data.push_back(static_cast<uint8_t>(value >> (8 * i)));
    }

    uint64_t unixTimeNow()
    {
        return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    }
}

// Restores the clock from a save, then catches it up on the time that passed
// while the game was off, as the cartridge's battery kept it running
void MBC3::loadClock(const std::vector<uint8_t> &footer)
{
    if (footer.size() < cartridge::RTC_SAVE_SIZE_OLD) return;
    for (size_t i = 0; i < 5; ++i)
    {
        const uint8_t select = static_cast<uint8_t>(cartridge::MBC3_RTC_SECONDS + i);
        setClockRegister(clock, select, static_cast<uint8_t>(readLittleEndian(footer, i * 4, 4)));
        setClockRegister(latched, select, static_cast<uint8_t>(readLittleEndian(footer, (5 + i) * 4, 4)));
    }

    const size_t timeBytes = footer.size() - cartridge::RTC_SAVE_REGISTER_COUNT * 4;
    const uint64_t savedAt = readLittleEndian(footer, cartridge::RTC_SAVE_REGISTER_COUNT * 4, timeBytes);
    const uint64_t now = unixTimeNow();
    if (now <= savedAt || (clock.daysHigh & cartridge::MBC3_HALT_BIT)) return;
    for (uint64_t second = 0; second < now - savedAt; ++second) advanceSecond();
}

std::vector<uint8_t> MBC3::saveExtra() const
{
    if (!cartridgeInfo->hasTimer) return {};
    std::vector<uint8_t> footer;
    for (const Clock *registers : {&clock, &latched})
    {
        for (uint8_t select = cartridge::MBC3_RTC_SECONDS; select <= cartridge::MBC3_RTC_DAYS_HIGH; ++select)
            writeLittleEndian(footer, clockRegisterValue(*registers, select), 4);
    }
    writeLittleEndian(footer, unixTimeNow(), 8);
    return footer;
}

uint8_t MBC5::read(const Address &address) const
{
    if (address.inRange(mmu::CARTRIDGE_ROM_ADDRESS_START, cartridge::ROM_BANK0_END)) return rom.at(address.getValue());
    if (address.inRange(cartridge::SWITCHABLE_ROM_BANK_START, cartridge::SWITCHABLE_ROM_BANK_END))
    {
        const size_t banks = rom.size() / cartridge::ROM_BANK_SIZE;
        return rom.at((banks ? romBank % banks : 0) * cartridge::ROM_BANK_SIZE + address.getValue() - cartridge::SWITCHABLE_ROM_BANK_START);
    }
    // RAM banks past the end of the RAM wrap around
    if (address.inRange(mmu::CARTRIDGE_RAM_ADDRESS_START, mmu::CARTRIDGE_RAM_ADDRESS_END) && ramEnabled && !ram.empty())
    {
        const size_t index = static_cast<size_t>(ramBank) * cartridge::RAM_BANK_SIZE + address.getValue() - mmu::CARTRIDGE_RAM_ADDRESS_START;
        return ram[index % ram.size()];
    }
    return io::UNMAPPED_READ_VALUE;
}

// https://gbdev.io/pandocs/MBC5.html#registers
void MBC5::write(const Address &address, uint8_t value)
{
    if (address.inRange(cartridge::RAM_ENABLE_START, cartridge::RAM_ENABLE_END)) ramEnabled = (value & cartridge::RAM_ENABLE_MASK) == cartridge::RAM_ENABLE_VALUE;
    else if (address.inRange(cartridge::ROM_BANK_SELECT_START, cartridge::MBC5_ROM_BANK_LOW_END)) romBank = (romBank & cartridge::MBC5_ROM_BANK_HIGH_BIT) | value;
    else if (address.inRange(cartridge::MBC5_ROM_BANK_HIGH_START, cartridge::ROM_BANK_SELECT_END)) romBank = (romBank & 0xFF) | ((value & 1) << 8); // Bit 0 is bank bit 8
    else if (address.inRange(cartridge::RAM_BANK_SELECT_START, cartridge::RAM_BANK_SELECT_END))
    {
        // Rumble carts use bit 3 to drive the motor rather than select RAM
        if (cartridgeInfo->hasRumble)
        {
            rumbleOn = (value & cartridge::MBC5_RUMBLE_BIT) != 0;
            ramBank = value & cartridge::MBC5_RUMBLE_BANK_MASK;
        }
        else
        {
            ramBank = value & cartridge::MBC5_RAM_BANK_MASK;
        }
    }
    else if (address.inRange(mmu::CARTRIDGE_RAM_ADDRESS_START, mmu::CARTRIDGE_RAM_ADDRESS_END) && ramEnabled && !ram.empty())
    {
        const size_t index = static_cast<size_t>(ramBank) * cartridge::RAM_BANK_SIZE + address.getValue() - mmu::CARTRIDGE_RAM_ADDRESS_START;
        ram[index % ram.size()] = value;
    }
}

