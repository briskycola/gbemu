#include "Cartridge.hpp"
#include "Address.hpp"
#include "CartridgeMetadata.hpp"
#include "File.hpp"
#include "MMU.hpp"
#include "IORegisters.hpp"
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
            throw std::runtime_error("Invalid RAM file: expected " + std::to_string(cartridgeRAMSize)
                + " bytes, got " + std::to_string(ram.size()));
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
    romBank.set(0x1); // The switchable ROM window starts at bank 1
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
    romBank.set(0x1); // The switchable ROM window starts at bank 1
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

uint8_t MBC1::read(const Address &address) const
{
    // Read into the first ROM bank
    if (address.inRange(mmu::CARTRIDGE_ROM_ADDRESS_START, cartridge::ROM_BANK0_END))
    {
        // In banking mode 1, the upper bank bits also apply to bank 0
        const uint32_t bank = bankingMode ? (romBank.getValue() & cartridge::MBC1_ROM_BANK_HIGH_MASK) : 0;
        const uint32_t bankCount = static_cast<uint32_t>(rom.size() / cartridge::ROM_BANK_SIZE);
        const uint32_t offset = bankCount ? (bank % bankCount) * cartridge::ROM_BANK_SIZE : 0;
        return rom.at(offset + address.getValue());
    }

    // Read into the second ROM Bank
    //
    // We need to do an offset for this one since
    // in the Game Boy, we can't directly access the
    // other 16 KB of the ROM.
    if (address.inRange(cartridge::SWITCHABLE_ROM_BANK_START, cartridge::SWITCHABLE_ROM_BANK_END))
    {
        uint16_t addressIntoBank;
        uint32_t bankOffset, addressInROM;
        addressIntoBank = address.getValue() - cartridge::SWITCHABLE_ROM_BANK_START;
        const uint32_t bankCount = static_cast<uint32_t>(rom.size() / cartridge::ROM_BANK_SIZE);
        const uint32_t bank = bankCount ? (romBank.getValue() % bankCount) : 0;
        bankOffset = cartridge::ROM_BANK_SIZE * bank;
        addressInROM = addressIntoBank + bankOffset;
        return rom.at(addressInROM);
    }

    if (address.inRange(mmu::CARTRIDGE_RAM_ADDRESS_START, mmu::CARTRIDGE_RAM_ADDRESS_END))
    {
        if (!ramEnabled || ram.empty()) return io::UNMAPPED_READ_VALUE;
        const uint32_t bankCount = (static_cast<uint32_t>(ram.size()) + cartridge::RAM_BANK_SIZE - 1) / cartridge::RAM_BANK_SIZE; // Round up
        const uint32_t bank = bankCount ? ramBank.getValue() % bankCount : 0;
        uint32_t offsetIntoRAM = cartridge::RAM_BANK_SIZE * bank;
        const uint32_t index = (offsetIntoRAM + address.getValue() - mmu::CARTRIDGE_RAM_ADDRESS_START) % ram.size();
        return ram[index];
    }
    throw std::runtime_error("Attempting to read from unmapped MBC1 address");
}

void MBC1::write(const Address &address, uint8_t value)
{
    if (address.inRange(cartridge::RAM_ENABLE_START, cartridge::RAM_ENABLE_END))
    {
        ramEnabled = (value & cartridge::RAM_ENABLE_MASK) == cartridge::RAM_ENABLE_VALUE;
    }

    // Bank 0 can't be selected for the switchable window
    if (address.inRange(cartridge::ROM_BANK_SELECT_START, cartridge::ROM_BANK_SELECT_END))
    {
        uint16_t lowBits = value & cartridge::MBC1_ROM_BANK_LOW_MASK;
        if (lowBits == 0) lowBits = 1;
        romBank.set((romBank.getValue() & cartridge::MBC1_ROM_BANK_HIGH_MASK) | lowBits);
    }

    if (address.inRange(cartridge::RAM_BANK_SELECT_START, cartridge::RAM_BANK_SELECT_END))
    {
        const uint16_t upperBits = (value & cartridge::MBC1_TWO_BIT_MASK) << cartridge::MBC1_ROM_BANK_HIGH_SHIFT;
        romBank.set(upperBits | (romBank.getValue() & cartridge::MBC1_ROM_BANK_LOW_MASK));
        ramBank.set(bankingMode ? (value & cartridge::MBC1_TWO_BIT_MASK) : 0);
    }

    // Mode 1 routes the 2-bit register to the RAM bank as well
    if (address.inRange(cartridge::BANKING_MODE_SELECT_START, cartridge::BANKING_MODE_SELECT_END))
    {
        bankingMode = (value & 0x01) != 0;
        ramBank.set(bankingMode ? ((romBank.getValue() >> cartridge::MBC1_ROM_BANK_HIGH_SHIFT) & cartridge::MBC1_TWO_BIT_MASK) : 0);
    }

    if (address.inRange(mmu::CARTRIDGE_RAM_ADDRESS_START, mmu::CARTRIDGE_RAM_ADDRESS_END))
    {
        if (!ramEnabled)
        {
            return;
        }

        else
        {
            const uint32_t bankCount = (static_cast<uint32_t>(ram.size()) + cartridge::RAM_BANK_SIZE - 1) / cartridge::RAM_BANK_SIZE; // Round up
            const uint32_t bank = bankCount ? ramBank.getValue() % bankCount : 0;
            uint32_t offsetIntoRAM = cartridge::RAM_BANK_SIZE * bank;
            const uint32_t index = (offsetIntoRAM + address.getValue() - mmu::CARTRIDGE_RAM_ADDRESS_START) % ram.size();
            ram[index] = value;
        }
    }
}

uint8_t MBC3::read(const Address &address) const
{
    // Read into the first ROM bank
    if (address.inRange(mmu::CARTRIDGE_ROM_ADDRESS_START, cartridge::ROM_BANK0_END))
    {
        return rom.at(address.getValue());
    }

    // Read into the second ROM Bank
    //
    // We need to do an offset for this one since
    // in the Game Boy, we can't directly access the
    // other 16 KB of the ROM.
    if (address.inRange(cartridge::SWITCHABLE_ROM_BANK_START, cartridge::SWITCHABLE_ROM_BANK_END))
    {
        uint16_t addressIntoBank;
        uint32_t bankOffset, addressInROM;
        addressIntoBank = address.getValue() - cartridge::SWITCHABLE_ROM_BANK_START;
        const uint32_t bankCount = static_cast<uint32_t>(rom.size() / cartridge::ROM_BANK_SIZE);
        const uint32_t bank = bankCount ? romBank.getValue() % bankCount : 0;
        bankOffset = cartridge::ROM_BANK_SIZE * bank;
        addressInROM = addressIntoBank + bankOffset;
        return rom.at(addressInROM);
    }

    if (address.inRange(mmu::CARTRIDGE_RAM_ADDRESS_START, mmu::CARTRIDGE_RAM_ADDRESS_END))
    {
        if (!ramEnabled || !isRAMOverRTC || ram.empty()) return io::UNMAPPED_READ_VALUE;
        const uint32_t bankCount = (static_cast<uint32_t>(ram.size()) + cartridge::RAM_BANK_SIZE - 1) / cartridge::RAM_BANK_SIZE; // Round up
        const uint32_t bank = bankCount ? ramBank.getValue() % bankCount : 0;
        const uint32_t index = bank * cartridge::RAM_BANK_SIZE + address.getValue() - mmu::CARTRIDGE_RAM_ADDRESS_START;
        return index < ram.size() ? ram[index] : io::UNMAPPED_READ_VALUE;
    }
    throw std::runtime_error("Attempting to read from unmapped MBC1 address");
}

void MBC3::write(const Address &address, uint8_t value)
{
    if (address.inRange(cartridge::RAM_ENABLE_START, cartridge::RAM_ENABLE_END))
    {
        ramEnabled = (value & cartridge::RAM_ENABLE_MASK) == cartridge::RAM_ENABLE_VALUE;
    }

    // Bank 0 can't be selected for the switchable window
    if (address.inRange(cartridge::ROM_BANK_SELECT_START, cartridge::ROM_BANK_SELECT_END))
    {
        uint16_t romBankBits = value & cartridge::MBC3_ROM_BANK_MASK;
        if (romBankBits == 0) romBankBits = 1;
        romBank.set(romBankBits);
    }

    if (address.inRange(cartridge::RAM_BANK_SELECT_START, cartridge::RAM_BANK_SELECT_END))
    {
        if (value <= cartridge::MBC3_MAX_RAM_BANK)
        {
            isRAMOverRTC = true;
            ramBank.set(value);
        }

        if (value >= cartridge::MBC3_RTC_START && value <= cartridge::MBC3_RTC_END)
        {
            isRAMOverRTC = false;
        }
    }

    // Latching the real-time clock is not implemented
    if (address.inRange(cartridge::BANKING_MODE_SELECT_START, cartridge::BANKING_MODE_SELECT_END))
    {
        //throw std::runtime_error("Not Implemented");
    }

    if (address.inRange(mmu::CARTRIDGE_RAM_ADDRESS_START, mmu::CARTRIDGE_RAM_ADDRESS_END))
    {
        if (!ramEnabled)
        {
            return;
        }

        if (isRAMOverRTC)
        {
            uint32_t offsetIntoRAM = cartridge::RAM_BANK_SIZE * ramBank.getValue();
            const uint32_t index = offsetIntoRAM + address.getValue() - mmu::CARTRIDGE_RAM_ADDRESS_START;
            if (index < ram.size()) ram[index] = value;
        }
    }
}

uint8_t MBC5::read(const Address &address) const
{
    if (address.inRange(mmu::CARTRIDGE_ROM_ADDRESS_START, cartridge::ROM_BANK0_END)) return rom.at(address.getValue());
    if (address.inRange(cartridge::SWITCHABLE_ROM_BANK_START, cartridge::SWITCHABLE_ROM_BANK_END))
    {
        const size_t banks = rom.size() / cartridge::ROM_BANK_SIZE;
        return rom.at((banks ? romBank % banks : 0) * cartridge::ROM_BANK_SIZE + address.getValue() - cartridge::SWITCHABLE_ROM_BANK_START);
    }
    if (address.inRange(mmu::CARTRIDGE_RAM_ADDRESS_START, mmu::CARTRIDGE_RAM_ADDRESS_END) && ramEnabled && !ram.empty())
    {
        const size_t index = static_cast<size_t>(ramBank) * cartridge::RAM_BANK_SIZE + address.getValue() - mmu::CARTRIDGE_RAM_ADDRESS_START;
        return index < ram.size() ? ram[index] : io::UNMAPPED_READ_VALUE;
    }
    return io::UNMAPPED_READ_VALUE;
}

void MBC5::write(const Address &address, uint8_t value)
{
    if (address.inRange(cartridge::RAM_ENABLE_START, cartridge::RAM_ENABLE_END)) ramEnabled = (value & cartridge::RAM_ENABLE_MASK) == cartridge::RAM_ENABLE_VALUE;
    else if (address.inRange(cartridge::ROM_BANK_SELECT_START, cartridge::MBC5_ROM_BANK_LOW_END)) romBank = (romBank & cartridge::MBC5_ROM_BANK_HIGH_BIT) | value;
    else if (address.inRange(cartridge::MBC5_ROM_BANK_HIGH_START, cartridge::ROM_BANK_SELECT_END)) romBank = (romBank & 0xFF) | ((value & 1) << 8); // Bit 0 is bank bit 8
    else if (address.inRange(cartridge::RAM_BANK_SELECT_START, cartridge::RAM_BANK_SELECT_END)) ramBank = value & cartridge::MBC5_RAM_BANK_MASK;
    else if (address.inRange(mmu::CARTRIDGE_RAM_ADDRESS_START, mmu::CARTRIDGE_RAM_ADDRESS_END) && ramEnabled && !ram.empty())
    {
        const size_t index = static_cast<size_t>(ramBank) * cartridge::RAM_BANK_SIZE + address.getValue() - mmu::CARTRIDGE_RAM_ADDRESS_START;
        if (index < ram.size()) ram[index] = value;
    }
}

