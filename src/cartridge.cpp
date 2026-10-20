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
            throw std::runtime_error("MBC2 is not implemented");
        case CartridgeType::MBC3:
            return std::make_shared<MBC3>(rom, ram, std::move(cartridgeInfo));
        case CartridgeType::MBC4:
            throw std::runtime_error("MBC4 is not implemented");
        case CartridgeType::MBC5:
            throw std::runtime_error("MBC5 is not implemented");
        case CartridgeType::NotImplemented:
            throw std::runtime_error("Not implemented");
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
        const uint32_t bank = bankingMode ? (romBank.getValue() & 0x60) : 0;
        const uint32_t bankCount = static_cast<uint32_t>(rom.size() / 0x4000);
        const uint32_t offset = bankCount ? (bank % bankCount) * 0x4000 : 0;
        return rom.at(offset + address.getValue());
    }

    // Read into the second ROM Bank
    //
    // We need to do an offset for this one since
    // in the Game Boy, we can't directly access the
    // other 16 KB of the ROM.
    if (address.inRange(0x4000, 0x7FFF))
    {
        uint16_t addressIntoBank;
        uint32_t bankOffset, addressInROM;
        addressIntoBank = address.getValue() - 0x4000;
        const uint32_t bankCount = static_cast<uint32_t>(rom.size() / 0x4000);
        const uint32_t bank = bankCount ? (romBank.getValue() % bankCount) : 0;
        bankOffset = 0x4000 * bank;
        addressInROM = addressIntoBank + bankOffset;
        return rom.at(addressInROM);
    }

    if (address.inRange(0xA000, 0xBFFF))
    {
        if (!isRAMEnabled || ram.empty()) return 0xFF;   // read case
        const uint32_t bankCount = (static_cast<uint32_t>(ram.size()) + 0x1FFF) / 0x2000;
        const uint32_t bank = bankCount ? ramBank.getValue() % bankCount : 0;
        uint32_t offsetIntoRAM = 0x2000 * bank;
        Address addressInRAM((address - 0xA000) + offsetIntoRAM);
        return ram.at(addressInRAM.getValue());
    }
    throw std::runtime_error("Attempting to read from unmapped MBC1 address");
}

void MBC1::write(const Address &address, uint8_t value)
{
    if (address.inRange(0x0000, 0x1FFF))
    {
        isRAMEnabled = (value & 0x0F) == 0x0A;
    }

    if (address.inRange(0x2000, 0x3FFF))
    {
        uint16_t lowBits = value & 0x1F;
        if (lowBits == 0) lowBits = 1;
        romBank.set((romBank.getValue() & 0x60) | lowBits);
    }

    if (address.inRange(0x4000, 0x5FFF))
    {
        const uint16_t upperBits = (value & 0x03) << 5;
        romBank.set(upperBits | (romBank.getValue() & 0x1F));
        ramBank.set(bankingMode ? (value & 0x03) : 0);
    }

    if (address.inRange(0x6000, 0x7FFF))
    {
        bankingMode = (value & 0x01) != 0;
        ramBank.set(bankingMode ? ((romBank.getValue() >> 5) & 0x03) : 0);
    }

    if (address.inRange(0xA000, 0xBFFF))
    {
        if (!isRAMEnabled)
        {
            return;
        }

        else
        {
            const uint32_t bankCount = (static_cast<uint32_t>(ram.size()) + 0x1FFF) / 0x2000;
            const uint32_t bank = bankCount ? ramBank.getValue() % bankCount : 0;
            uint32_t offsetIntoRAM = 0x2000 * bank;
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
        uint16_t addressIntoBank;
        uint32_t bankOffset, addressInROM;
        addressIntoBank = address.getValue() - 0x4000;
        bankOffset = 0x4000 * romBank.getValue();
        addressInROM = addressIntoBank + bankOffset;
        return rom.at(addressInROM);
    }

    if (address.inRange(0xA000, 0xBFFF))
    {
        //if (!isRAMEnabled || ram.empty()) return 0xFF;   // read case
        uint32_t offsetIntoRAM = 0x2000 * ramBank.getValue();
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

        if (value == 0x0)
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

        uint16_t romBankBits = value & 0x7F;
        romBank.set(romBankBits);
    }

    if (address.inRange(0x4000, 0x5FFF))
    {
        if (value <= 0x03)
        {
            isRAMOverRTC = true;
            ramBank.set(value);
        }

        if (value >= 0x08 && value <= 0xC)
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
        if (!isRAMEnabled)
        {
            return;
        }

        if (isRAMOverRTC)
        {
            uint32_t offsetIntoRAM = 0x2000 * ramBank.getValue();
            Address addressInRAM((address - 0xA000) + offsetIntoRAM);
            ram.at(addressInRAM.getValue()) = value;
        }
    }
}

