#include "cartridge_metadata.hpp"
#include <iostream>
#include <vector>
#include <string>
#include <memory>

CartridgeType getCartridgeType(uint8_t type)
{
    switch (type)
    {
        case 0x00: case 0x08: case 0x09:
            return CartridgeType::ROMOnly;
        case 0x01: case 0x02: case 0x03:
        case 0xFF:
            return CartridgeType::MBC1;
        case 0x05: case 0x06:
            return CartridgeType::MBC2;
        case 0x0F: case 0x10: case 0x11:
        case 0x12: case 0x13:
            return CartridgeType::MBC3;
        case 0x15: case 0x16: case 0x17:
            return CartridgeType::MBC4;
        case 0x19: case 0x1A: case 0x1B:
        case 0x1C: case 0x1D: case 0x1E:
            return CartridgeType::MBC5;
        case 0x0B: case 0x0C: case 0x0D:
        case 0x20: case 0x22: case 0xFC:
        case 0xFD: case 0xFE:
            return CartridgeType::NotImplemented;
        default:
            return CartridgeType::NotImplemented;
    }
}

std::string describeCartridge(CartridgeType type)
{
    switch (type)
    {
        case CartridgeType::ROMOnly:
            return "ROM Only";
        case CartridgeType::MBC1:
            return "MBC1";
        case CartridgeType::MBC2:
            return "MBC2";
        case CartridgeType::MBC3:
            return "MBC3";
        case CartridgeType::MBC4:
            return "MBC4";
        case CartridgeType::MBC5:
            return "MBC5";
        case CartridgeType::NotImplemented:
            return "Not Implemented";
        default:
            return "Not Implemented";
    }
}

std::string getTitleFromCartridge(const std::vector<uint8_t> &rom)
{
    char name[TITLE_LENGTH];

    for (uint8_t i = 0; i < TITLE_LENGTH; i++)
    {
        name[i] = static_cast<char>(rom[TITLE_START_ADDRESS + i]);
    }

    return std::string(name, TITLE_LENGTH);
}

ROMSize getRomSize(uint8_t size)
{
    switch (size)
    {
        case 0x00:
            return ROMSize::KB32;
        case 0x01:
            return ROMSize::KB64;
        case 0x02:
            return ROMSize::KB128;
        case 0x03:
            return ROMSize::KB256;
        case 0x04:
            return ROMSize::KB512;
        case 0x05:
            return ROMSize::MB1;
        case 0x06:
            return ROMSize::MB2;
        case 0x07:
            return ROMSize::MB4;
        case 0x52:
            return ROMSize::MB1p1;
        case 0x53:
            return ROMSize::MB1p2;
        case 0x54:
            return ROMSize::MB1p5;
        default:
            return ROMSize::KB32;
    }
}

std::string describeROM(ROMSize size)
{
    switch (size)
    {
        case ROMSize::KB32:
            return "32KB (no ROM banking)";
        case ROMSize::KB64:
            return "64KB (4 banks)";
        case ROMSize::KB128:
            return "128KB (8 banks)";
        case ROMSize::KB256:
            return "256KB (16 banks)";
        case ROMSize::KB512:
            return "512KB (32 banks)";
        case ROMSize::MB1:
            return "1MB (64 banks)";
        case ROMSize::MB2:
            return "2MB (128 banks)";
        case ROMSize::MB4:
            return "4MB (256 banks)";
        case ROMSize::MB1p1:
            return "1.1MB (72 banks)";
        case ROMSize::MB1p2:
            return "1.2MB (80 banks)";
        case ROMSize::MB1p5:
            return "1.5MB (96 banks)";
        default:
            return "Unknown ROM size";
    }
}

RAMSize getRamSize(uint8_t size)
{
    switch (size)
    {
        case 0x00:
            return RAMSize::None;
        case 0x01:
            return RAMSize::KB2;
        case 0x02:
            return RAMSize::KB8;
        case 0x03:
            return RAMSize::KB32;
        case 0x04:
            return RAMSize::KB128;
        case 0x05:
            return RAMSize::KB64;
        default:
            return RAMSize::None;
    }
}

uint32_t getActualRamSize(RAMSize size)
{
    switch (size)
    {
        case RAMSize::None:
            return 0x0;
        case RAMSize::KB2:
            return 0x800;
        case RAMSize::KB8:
            return 0x2000;
        case RAMSize::KB32:
            return 0x8000;
        case RAMSize::KB64:
            return 0x10000;
        case RAMSize::KB128:
            return 0x20000;
        default:
            return 0x0;
    }
}

std::string describeRAM(RAMSize size)
{
    switch (size)
    {
        case RAMSize::None:
            return "No RAM";
        case RAMSize::KB2:
            return "2KB";
        case RAMSize::KB8:
            return "8KB";
        case RAMSize::KB32:
            return "32KB";
        case RAMSize::KB128:
            return "128KB";
        case RAMSize::KB64:
            return "64KB";
        default:
            return "Unknown RAM size";
    }
}

Destination getDestination(uint8_t destination)
{
    switch (destination)
    {
        case 0x00:
            return Destination::Japanese;
        case 0x01:
            return Destination::NonJapanese;
        default:
            return Destination::NonJapanese;
    }
}

std::string describeDestination(Destination destination)
{
    switch (destination)
    {
        case Destination::Japanese:
            return "Japanese";
        case Destination::NonJapanese:
            return "Non Japanese";
        default:
            return "Non Japanese";
    }
}

std::unique_ptr<CartridgeInfo> getInfo(const std::vector<uint8_t> &rom)
{
    std::unique_ptr<CartridgeInfo> info;
    info = std::make_unique<CartridgeInfo>();

    info->title = getTitleFromCartridge(rom);
    info->type = getCartridgeType(rom[CARTRIDGE_TYPE_START_ADDRESS]);
    info->version = ROM_VERSION_NUMBER_START_ADDRESS;
    info->romSize = getRomSize(rom[ROM_SIZE_START_ADDRESS]);
    info->ramSize = getRamSize(rom[RAM_SIZE_START_ADDRESS]);

    std::cout << "Title: " << info->title << "\n";
    std::cout << "Type: " << describeCartridge(info->type) << "\n";
    std::cout << "Version: " << std::hex << info->version << std::dec << "\n";
    std::cout << "ROM Size: " << describeROM(info->romSize) << "\n";
    std::cout << "RAM Size: " << describeRAM(info->ramSize) << "\n";
    return info;
}
