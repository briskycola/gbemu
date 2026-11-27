#include "Core/Cartridge/CartridgeMetadata.hpp"
#include <algorithm>
#include <map>
#include <vector>
#include <string>
#include <memory>

// The header code tables below follow
// https://gbdev.io/pandocs/The_Cartridge_Header.html

// Header byte 0x147. Each case lists the codes for one
// controller, with and without RAM, battery, and so on.
// https://gbdev.io/pandocs/The_Cartridge_Header.html#0147--cartridge-type
CartridgeType getCartridgeType(uint8_t type)
{
    switch (type)
    {
        case 0x00: case 0x08: case 0x09:
            return CartridgeType::ROMOnly;
        // $FF is HuC1+RAM+BATTERY, which banks like an MBC1, so it runs as
        // one (without the HuC1's infrared port)
        case 0x01: case 0x02: case 0x03:
        case 0xFF:
            return CartridgeType::MBC1;
        case 0x05: case 0x06:
            return CartridgeType::MBC2;
        case 0x0F: case 0x10: case 0x11:
        case 0x12: case 0x13:
            return CartridgeType::MBC3;
        case 0x19: case 0x1A: case 0x1B:
        case 0x1C: case 0x1D: case 0x1E:
            return CartridgeType::MBC5;
        default:
            return CartridgeType::Unsupported;
    }
}

// The names Pan Docs gives each code
// https://gbdev.io/pandocs/The_Cartridge_Header.html#0147--cartridge-type
std::string cartridgeTypeName(uint8_t type)
{
    static const std::map<uint8_t, std::string> NAMES =
    {
        {0x00, "ROM ONLY"},
        {0x01, "MBC1"}, {0x02, "MBC1+RAM"}, {0x03, "MBC1+RAM+BATTERY"},
        {0x05, "MBC2"}, {0x06, "MBC2+BATTERY"},
        {0x08, "ROM+RAM"}, {0x09, "ROM+RAM+BATTERY"},
        {0x0B, "MMM01"}, {0x0C, "MMM01+RAM"}, {0x0D, "MMM01+RAM+BATTERY"},
        {0x0F, "MBC3+TIMER+BATTERY"}, {0x10, "MBC3+TIMER+RAM+BATTERY"},
        {0x11, "MBC3"}, {0x12, "MBC3+RAM"}, {0x13, "MBC3+RAM+BATTERY"},
        {0x19, "MBC5"}, {0x1A, "MBC5+RAM"}, {0x1B, "MBC5+RAM+BATTERY"},
        {0x1C, "MBC5+RUMBLE"}, {0x1D, "MBC5+RUMBLE+RAM"}, {0x1E, "MBC5+RUMBLE+RAM+BATTERY"},
        {0x20, "MBC6"},
        {0x22, "MBC7+SENSOR+RUMBLE+RAM+BATTERY"},
        {0xFC, "POCKET CAMERA"},
        {0xFD, "BANDAI TAMA5"},
        {0xFE, "HuC3"},
        {0xFF, "HuC1+RAM+BATTERY"},
    };
    const auto found = NAMES.find(type);
    return found == NAMES.end() ? std::string() : found->second;
}

std::string unsupportedCartridgeMessage(uint8_t type)
{
    static const std::string HEX_DIGITS = "0123456789ABCDEF";
    const std::string code = std::string("$") + HEX_DIGITS[type >> 4] + HEX_DIGITS[type & 0x0F];
    const std::string name = cartridgeTypeName(type);
    if (name.empty())
        return "This game's cartridge type, " + code + ", isn't one Pan Docs lists.";
    return "This game's cartridge is " + name + " (type " + code + "), which gbemu doesn't support.";
}

// Cartridge types with a battery: MBC1, MBC2, ROM, MMM01, MBC3 (with or without
// the timer), MBC5 (with or without rumble), MBC7 and HuC1
bool cartridgeHasBattery(uint8_t type)
{
    switch (type)
    {
        case 0x03: case 0x06: case 0x09: case 0x0D: case 0x0F: case 0x10:
        case 0x13: case 0x1B: case 0x1E: case 0x22: case 0xFF:
            return true;
        default:
            return false;
    }
}

// MBC3+TIMER+BATTERY and MBC3+TIMER+RAM+BATTERY
bool cartridgeHasTimer(uint8_t type)
{
    return type == 0x0F || type == 0x10;
}

// MBC5+RUMBLE, with or without RAM and battery
bool cartridgeHasRumble(uint8_t type)
{
    return type == 0x1C || type == 0x1D || type == 0x1E;
}

// The title fills $0134-$0143, padded with zero bytes. On games with a CGB
// flag, $0143 is that flag, and on newer ones $013F-$0142 hold a 4-character
// manufacturer code. Pan Docs gives no way to tell whether a game has one,
// so on CGB games four uppercase letters or digits there are taken as one.
// https://gbdev.io/pandocs/The_Cartridge_Header.html#0134-0143--title
std::string getTitleFromCartridge(const std::vector<uint8_t> &rom)
{
    const bool hasCGBFlag = (rom[cartridge::CGB_FLAG_START_ADDRESS] & cartridge::CGB_SUPPORT_FLAG) != 0;
    const auto manufacturerCode = rom.begin() + cartridge::MANUFACTURER_CODE_START_ADDRESS;
    const bool hasManufacturerCode = hasCGBFlag && std::all_of(
        manufacturerCode, manufacturerCode + cartridge::MANUFACTURER_CODE_LENGTH,
        [](uint8_t character) { return (character >= 'A' && character <= 'Z') || (character >= '0' && character <= '9'); });

    const size_t length = hasManufacturerCode ? cartridge::TITLE_LENGTH_WITH_MANUFACTURER_CODE
                        : hasCGBFlag ? cartridge::TITLE_LENGTH_WITH_CGB_FLAG
                        : cartridge::TITLE_LENGTH;
    const auto start = rom.begin() + cartridge::TITLE_START_ADDRESS;
    return std::string(start, std::find(start, start + length, 0));
}

// Header byte 0x148
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
            return "32 KiB (no banking)";
        case ROMSize::KB64:
            return "64 KiB (4 banks)";
        case ROMSize::KB128:
            return "128 KiB (8 banks)";
        case ROMSize::KB256:
            return "256 KiB (16 banks)";
        case ROMSize::KB512:
            return "512 KiB (32 banks)";
        case ROMSize::MB1:
            return "1 MiB (64 banks)";
        case ROMSize::MB2:
            return "2 MiB (128 banks)";
        case ROMSize::MB4:
            return "4 MiB (256 banks)";
        case ROMSize::MB1p1:
            return "1.1 MiB (72 banks)";
        case ROMSize::MB1p2:
            return "1.2 MiB (80 banks)";
        case ROMSize::MB1p5:
            return "1.5 MiB (96 banks)";
        default:
            return "Unknown ROM size";
    }
}

// Header byte 0x149
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

// Size of each RAM option in bytes
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
            return "2 KiB";
        case RAMSize::KB8:
            return "8 KiB";
        case RAMSize::KB32:
            return "32 KiB";
        case RAMSize::KB128:
            return "128 KiB";
        case RAMSize::KB64:
            return "64 KiB";
        default:
            return "Unknown RAM size";
    }
}

// Header byte 0x14A
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
    info->typeCode = rom[cartridge::CARTRIDGE_TYPE_START_ADDRESS];
    info->type = getCartridgeType(info->typeCode);
    info->supportsCGB = (rom[cartridge::CGB_FLAG_START_ADDRESS] & cartridge::CGB_SUPPORT_FLAG) != 0;
    info->cgbOnly = rom[cartridge::CGB_FLAG_START_ADDRESS] == cartridge::CGB_ONLY_FLAG;
    info->supportsSGB = rom[cartridge::SGB_FLAG_START_ADDRESS] == cartridge::SGB_SUPPORT_FLAG &&
        rom[cartridge::OLD_LICENSEE_CODE_START_ADDRESS] == cartridge::USE_NEW_LICENSEE_CODE_FLAG;
    info->version = rom[cartridge::ROM_VERSION_NUMBER_START_ADDRESS];
    info->hasBattery = cartridgeHasBattery(rom[cartridge::CARTRIDGE_TYPE_START_ADDRESS]);
    info->hasTimer = cartridgeHasTimer(rom[cartridge::CARTRIDGE_TYPE_START_ADDRESS]);
    info->hasRumble = cartridgeHasRumble(rom[cartridge::CARTRIDGE_TYPE_START_ADDRESS]);
    info->romSize = getRomSize(rom[cartridge::ROM_SIZE_START_ADDRESS]);
    info->ramSize = getRamSize(rom[cartridge::RAM_SIZE_START_ADDRESS]);
    return info;
}
