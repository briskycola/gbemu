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
    uint32_t cartridgeRAMSize = getActualRamSize(this->cartridgeInfo->ramSize);

    if (!ram.empty())
    {
        if (ram.size() != cartridgeRAMSize)
        {
            std::cerr << "Invalid RAM File\n";
            exit(1);
        }
        this->ram = ram;
    }

    else
    {
        this->ram = std::vector<uint8_t>(cartridgeRAMSize, 0);
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
    if (address.inRange(0x0000, 0x7FFF))
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
    if (address.inRange(0x0000, 0x3FFF))
    {
        return rom.at(address.getValue());
    }

    // Read into the second ROM Bank
    //
    // We need to do an offset for this one since
    // in the Game Boy, we can't directly access the
    // other 16 KB of the ROM.
    if (address.inRange(0x4000, 0x7FFF))
    {
        uint16_t addressIntoBank, bankOffset, addressInROM;
        addressIntoBank = address.getValue() - 0x4000;
        bankOffset = 0x4000 * romBank.getValue();
        addressInROM = addressIntoBank + bankOffset;
        return rom.at(addressInROM);
    }

    if (address.inRange(0xA000, 0xBFFF))
    {
        if (!isRAMEnabled || ram.empty()) return 0xFF;   // read case
        uint16_t offsetIntoRAM = 0x2000 * ramBank.getValue();
        Address addressInRAM((address - 0xA000) + offsetIntoRAM);
        return ram.at(addressInRAM.getValue());
    }
    throw std::runtime_error("Attempting to read from unmapped MBC1 address");
}

void MBC1::write(const Address &address, uint8_t value)
{
    if (address.inRange(0x0000, 0x1FFF))
    {
        isRAMEnabled = true;
    }

    if (address.inRange(0x2000, 0x3FFF))
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

    if (address.inRange(0x4000, 0x5FFF))
    {
        //throw std::runtime_error("Not Implemented");
    }

    if (address.inRange(0x6000, 0x7FFF))
    {
        //throw std::runtime_error("Not Implemented");
    }

    if (address.inRange(0xA000, 0xBFFF))
    {
        if (!isRAMEnabled || ram.empty())
        {
            return;
        }

        else
        {
            uint16_t offsetIntoRAM = 0x2000 * ramBank.getValue();
            Address addressInRAM((address - 0xA000) + offsetIntoRAM);
            ram.at(addressInRAM.getValue()) = value;
        }
    }
}

uint8_t MBC3::read(const Address &address) const
{
    // Read into the first ROM bank
    if (address.inRange(0x0000, 0x3FFF))
    {
        return rom.at(address.getValue());
    }

    // Read into the second ROM Bank
    //
    // We need to do an offset for this one since
    // in the Game Boy, we can't directly access the
    // other 16 KB of the ROM.
    if (address.inRange(0x4000, 0x7FFF))
    {
        uint16_t addressIntoBank, bankOffset, addressInROM;
        addressIntoBank = address.getValue() - 0x4000;
        bankOffset = 0x4000 * romBank.getValue();
        addressInROM = addressIntoBank + bankOffset;
        return rom.at(addressInROM);
    }

    if (address.inRange(0xA000, 0xBFFF))
    {
        if (!isRAMEnabled || ram.empty()) return 0xFF;   // read case
        uint16_t offsetIntoRAM = 0x2000 * ramBank.getValue();
        Address addressInRAM((address - 0xA000) + offsetIntoRAM);
        return ram.at(addressInRAM.getValue());
    }
    throw std::runtime_error("Attempting to read from unmapped MBC1 address");
}

void MBC3::write(const Address &address, uint8_t value)
{
    if (address.inRange(0x0000, 0x1FFF))
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

    if (address.inRange(0x2000, 0x3FFF))
    {
        if (value == 0x0)
        {
            romBank.set(0x1);
        }
        romBank.set(value & 0x7F);
    }

    if (address.inRange(0x4000, 0x5FFF))
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

    if (address.inRange(0x6000, 0x7FFF))
    {
        //throw std::runtime_error("Not Implemented");
    }

    if (address.inRange(0xA000, 0xBFFF))
    {
        if (!isRAMEnabled || ram.empty())
        {
            return;
        }

        if (isRAMOverRTC)
        {
            uint16_t offsetIntoRAM = 0x2000 * ramBank.getValue();
            Address addressInRAM((address - 0xA000) + offsetIntoRAM);
            ram.at(addressInRAM.getValue()) = value;
        }
    }
}

