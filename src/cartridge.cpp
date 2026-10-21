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
            std::runtime_error("Unknown cartridge type");
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
    if (address.inRange(0x0000, 0x7FFF))
    {
        return rom.at(address.getValue());
    }

    else if (address.inRange(0xA000, 0xBFFF))
    {
        return ram.empty() ? 0xFF : ram.at((address.getValue() - 0xA000) % ram.size());
    }
    else
    {
        std::runtime_error("Invalid address for NoMBC cartridge");
    }

    return 0xFF;
}

void NoMBC::write(const Address &address, uint8_t value)
{
    if (address.inRange(0xA000, 0xBFFF) && !ram.empty())
        ram.at((address.getValue() - 0xA000) % ram.size()) = value;
}

MBC2::MBC2(std::vector<uint8_t> rom, const std::vector<uint8_t> &save,
           std::unique_ptr<CartridgeInfo> cartridgeInfo) :
    Cartridge(std::move(rom), {}, std::move(cartridgeInfo)), nibbleRAM(0x200, 0x0F)
{
    if (!save.empty())
    {
        if (save.size() != nibbleRAM.size()) throw std::runtime_error("Invalid MBC2 save size");
        for (size_t i = 0; i < nibbleRAM.size(); ++i) nibbleRAM[i] = save[i] & 0x0F;
    }
}

uint8_t MBC2::read(const Address &address) const
{
    if (address.inRange(0x0000, 0x3FFF)) return rom.at(address.getValue());
    if (address.inRange(0x4000, 0x7FFF))
    {
        const size_t banks = rom.size() / 0x4000;
        return rom.at((banks ? romBank % banks : 0) * 0x4000 + address.getValue() - 0x4000);
    }
    if (address.inRange(0xA000, 0xBFFF) && ramEnabled)
        return 0xF0 | nibbleRAM[(address.getValue() - 0xA000) & 0x1FF];
    return 0xFF;
}

void MBC2::write(const Address &address, uint8_t value)
{
    if (address.inRange(0x0000, 0x3FFF))
    {
        if ((address.getValue() & 0x0100) == 0) ramEnabled = (value & 0x0F) == 0x0A;
        else { romBank = value & 0x0F; if (romBank == 0) romBank = 1; }
    }
    else if (address.inRange(0xA000, 0xBFFF) && ramEnabled)
        nibbleRAM[(address.getValue() - 0xA000) & 0x1FF] = value & 0x0F;
}

const std::vector<uint8_t>& MBC2::getCartridgeRAM() const { return nibbleRAM; }

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
        const uint32_t index = (offsetIntoRAM + address.getValue() - 0xA000) % ram.size();
        return ram[index];
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
            const uint32_t index = (offsetIntoRAM + address.getValue() - 0xA000) % ram.size();
            ram[index] = value;
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
        const uint32_t bankCount = static_cast<uint32_t>(rom.size() / 0x4000);
        const uint32_t bank = bankCount ? romBank.getValue() % bankCount : 0;
        bankOffset = 0x4000 * bank;
        addressInROM = addressIntoBank + bankOffset;
        return rom.at(addressInROM);
    }

    if (address.inRange(0xA000, 0xBFFF))
    {
        if (!isRAMEnabled || !isRAMOverRTC || ram.empty()) return 0xFF;
        const uint32_t bankCount = (static_cast<uint32_t>(ram.size()) + 0x1FFF) / 0x2000;
        const uint32_t bank = bankCount ? ramBank.getValue() % bankCount : 0;
        const uint32_t index = bank * 0x2000 + address.getValue() - 0xA000;
        return index < ram.size() ? ram[index] : 0xFF;
    }
    throw std::runtime_error("Attempting to read from unmapped MBC1 address");
}

void MBC3::write(const Address &address, uint8_t value)
{
    if (address.inRange(0x0000, 0x1FFF))
    {
        isRAMEnabled = (value & 0x0F) == 0x0A;
    }

    if (address.inRange(0x2000, 0x3FFF))
    {
        uint16_t romBankBits = value & 0x7F;
        if (romBankBits == 0) romBankBits = 1;
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
            const uint32_t index = offsetIntoRAM + address.getValue() - 0xA000;
            if (index < ram.size()) ram[index] = value;
        }
    }
}

MBC5::MBC5(std::vector<uint8_t> rom, const std::vector<uint8_t> &ram,
           std::unique_ptr<CartridgeInfo> cartridgeInfo) :
    Cartridge(std::move(rom), ram, std::move(cartridgeInfo)) {}

uint8_t MBC5::read(const Address &address) const
{
    if (address.inRange(0x0000, 0x3FFF)) return rom.at(address.getValue());
    if (address.inRange(0x4000, 0x7FFF))
    {
        const size_t banks = rom.size() / 0x4000;
        return rom.at((banks ? romBank % banks : 0) * 0x4000 + address.getValue() - 0x4000);
    }
    if (address.inRange(0xA000, 0xBFFF) && ramEnabled && !ram.empty())
    {
        const size_t index = static_cast<size_t>(ramBank) * 0x2000 + address.getValue() - 0xA000;
        return index < ram.size() ? ram[index] : 0xFF;
    }
    return 0xFF;
}

void MBC5::write(const Address &address, uint8_t value)
{
    if (address.inRange(0x0000, 0x1FFF)) ramEnabled = (value & 0x0F) == 0x0A;
    else if (address.inRange(0x2000, 0x2FFF)) romBank = (romBank & 0x100) | value;
    else if (address.inRange(0x3000, 0x3FFF)) romBank = (romBank & 0xFF) | ((value & 1) << 8);
    else if (address.inRange(0x4000, 0x5FFF)) ramBank = value & 0x0F;
    else if (address.inRange(0xA000, 0xBFFF) && ramEnabled && !ram.empty())
    {
        const size_t index = static_cast<size_t>(ramBank) * 0x2000 + address.getValue() - 0xA000;
        if (index < ram.size()) ram[index] = value;
    }
}

