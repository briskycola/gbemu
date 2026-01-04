#include "cartridge.hpp"
#include "address.hpp"
#include "cartridge_metadata.hpp"
#include "file.hpp"
#include <iostream>

Cartridge::Cartridge(std::vector<uint8_t> rom, const std::vector<uint8_t> &ram,
                     std::unique_ptr<CartridgeInfo> cartridgeInfo) :
    rom(std::move(rom)),
    ram(std::move(ram)),
    cartridgeInfo(std::move(cartridgeInfo))
{
    uint32_t cartridgeRAMSize = getActualRamSize(cartridgeInfo->ramSize);

    if (!ram.empty())
    {
        if (ram.size() != cartridgeRAMSize)
        {
            std::cerr << "Invalid RAM File\n";
            exit(1);
        }

        else
        {
            this->ram = std::vector<uint8_t>(cartridgeRAMSize, 0);
        }
    }
}

NoMBC::NoMBC(std::vector<uint8_t> rom, const std::vector<uint8_t> &ram,
             std::unique_ptr<CartridgeInfo> cartridgeInfo) :
    Cartridge(std::move(rom), ram, std::move(cartridgeInfo))
{}

MBC1::MBC1(std::vector<uint8_t> rom, const std::vector<uint8_t> &ram,
             std::unique_ptr<CartridgeInfo> cartridgeInfo) :
    Cartridge(std::move(rom), ram, std::move(cartridgeInfo))
{
    romBank.set(0x1);
}

MBC3::MBC3(std::vector<uint8_t> rom, const std::vector<uint8_t> &ram,
             std::unique_ptr<CartridgeInfo> cartridgeInfo) :
    Cartridge(std::move(rom), ram, std::move(cartridgeInfo))
{
    romBank.set(0x1);
}

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
            std::runtime_error("MBC2 is not implemented");
        case CartridgeType::MBC3:
            return std::make_shared<MBC3>(rom, ram, std::move(cartridgeInfo));
        case CartridgeType::MBC4:
            std::runtime_error("MBC4 is not implemented");
        case CartridgeType::MBC5:
            std::runtime_error("MBC5 is not implemented");
        case CartridgeType::NotImplemented:
            std::runtime_error("Not implemented");
        default:
            std::runtime_error("Unknown cartridge type");
    }
    return nullptr;
}

const std::vector<uint8_t>& Cartridge::getCartridgeRAM() const
{
    return ram;
}

uint8_t NoMBC::read(const Address &address) const
{
    if (address.inRange(NOMBC_ROM_START, NOMBC_ROM_END))
    {
        return rom.at(address.getValue());
    }

    else
    {
        std::runtime_error("Invalid address for NoMBC cartridge");
    }

    return 0x0;
}

void NoMBC::write(const Address &address, uint8_t value)
{
    return;
}

uint8_t MBC1::read(const Address &address) const
{
    // Read into the first ROM bank
    if (address.inRange(MBC1_ROM_ROMBANK_X0_START, MBC1_ROM_ROMBANK_X0_END))
    {
        return rom.at(address.getValue());
    }

    // Read into the second ROM Bank
    //
    // We need to do an offset for this one since
    // in the Game Boy, we can't directly access the
    // other 16 KB of the ROM.
    if (address.inRange(MBC1_ROM_ROMBANK_01_7F_START, MBC1_ROM_ROMBANK_01_7F_END))
    {
        uint16_t addressIntoBank, bankOffset, addressInROM;
        addressIntoBank = address.getValue() - MBC1_ROM_ROMBANK_01_7F_START;
        bankOffset = MBC1_ROM_ROMBANK_01_7F_START * romBank.getValue();
        addressInROM = addressIntoBank + bankOffset;
        return rom.at(addressInROM);
    }

    if (address.inRange(MBC1_ROM_RAMBANK_START, MBC1_ROM_RAMBANK_END))
    {
        uint16_t offsetIntoRAM = MBC1_RAM_RAMBANK_START * ramBank.getValue();
        Address addressInRAM((address - MBC1_ROM_RAMBANK_START) + offsetIntoRAM);
        return ram.at(addressInRAM.getValue());
    }
    return 0x0;
}

void MBC1::write(const Address &address, uint8_t value)
{
    if (address.inRange(MBC1_RAM_ENABLE_START, MBC1_RAM_ENABLE_END))
    {
        isRAMEnabled = true;
    }

    if (address.inRange(MBC1_RAM_ROMBANK_START, MBC1_RAM_ROMBANK_END))
    {
        switch (value)
        {
            case 0x0:
                romBank.set(0x1);
                return;
            case 0x20:
                romBank.set(0x21);
                return;
            case 0x40:
                romBank.set(0x41);
                return;
            case 0x60:
                romBank.set(0x61);
                return;
        }

        romBank.set(value & 0x1F);
    }

    if (address.inRange(MBC1_RAM_RAMBANK_START, MBC1_RAM_RAMBANK_END))
    {
        throw std::runtime_error("Not Implemented");
    }

    if (address.inRange(MBC1_RAM_BANKINGMODE_START, MBC1_RAM_BANKINGMODE_END))
    {
        throw std::runtime_error("Not Implemented");
    }

    if (address.inRange(MBC1_ROM_RAMBANK_START, MBC1_ROM_RAMBANK_END))
    {
        if (!isRAMEnabled)
        {
            return;
        }

        else
        {
            uint16_t offsetIntoRAM = MBC1_RAM_RAMBANK_START * ramBank.getValue();
            Address addressInRAM((address - MBC1_ROM_RAMBANK_START) + offsetIntoRAM);
            value = ram.at(addressInRAM.getValue());
        }
    }
}

uint8_t MBC3::read(const Address &address) const
{
    // Read into the first ROM bank
    if (address.inRange(MBC3_ROM_ROMBANK_00_START, MBC3_ROM_ROMBANK_00_END))
    {
        return rom.at(address.getValue());
    }

    // Read into the second ROM Bank
    //
    // We need to do an offset for this one since
    // in the Game Boy, we can't directly access the
    // other 16 KB of the ROM.
    if (address.inRange(MBC3_ROM_ROMBANK_01_7F_START, MBC3_ROM_ROMBANK_01_7F_END))
    {
        uint16_t addressIntoBank, bankOffset, addressInROM;
        addressIntoBank = address.getValue() - MBC3_ROM_ROMBANK_01_7F_START;
        bankOffset = MBC3_ROM_ROMBANK_01_7F_START * romBank.getValue();
        addressInROM = addressIntoBank + bankOffset;
        return rom.at(addressInROM);
    }

    if (address.inRange(MBC3_ROM_RAMBANK_REGISTER_START, MBC3_ROM_RAMBANK_REGISTER_END))
    {
        uint16_t offsetIntoRAM = MBC3_RAM_RAMBANK_START * ramBank.getValue();
        Address addressInRAM((address - MBC3_ROM_RAMBANK_REGISTER_START) + offsetIntoRAM);
        return ram.at(addressInRAM.getValue());
    }
    return 0x0;
}

void MBC3::write(const Address &address, uint8_t value)
{
    if (address.inRange(MBC3_RAM_ENABLE_TIMER_START, MBC3_RAM_ENABLE_TIMER_END))
    {
        if (value == 0x0A)
        {
            isRAMEnabled = true;
        }

        else
        {
            isRAMEnabled = false;
        }
    }

    if (address.inRange(MBC3_RAM_ROMBANK_START, MBC3_RAM_ROMBANK_END))
    {
        if (value == 0x0)
        {
            romBank.set(0x1);
        }
        romBank.set(value & 0x7F);
    }

    if (address.inRange(MBC3_RAM_RAMBANK_START, MBC3_RAM_RAMBANK_END))
    {
        if (value <= 0x03)
        {
            isRAMOverRTC = true;
            ramBank.set(value);
        }

        if (value >= 0x08 && value <= 0x0C)
        {
            isRAMOverRTC = false;
        }
    }

    if (address.inRange(MBC3_RAM_LATCH_CLOCK_START, MBC3_RAM_LATCH_CLOCK_END))
    {
        throw std::runtime_error("Not Implemented");
    }

    if (address.inRange(MBC3_ROM_RAMBANK_REGISTER_START, MBC3_ROM_RAMBANK_REGISTER_END))
    {
        if (!isRAMEnabled)
        {
            return;
        }

        if (isRAMOverRTC)
        {
            uint16_t offsetIntoRAM = MBC3_RAM_RAMBANK_START * ramBank.getValue();
            Address addressInRAM((address - MBC3_ROM_RAMBANK_REGISTER_START) + offsetIntoRAM);
            value = ram.at(addressInRAM.getValue());
        }
    }
}

