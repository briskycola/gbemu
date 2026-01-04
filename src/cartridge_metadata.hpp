#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>

static constexpr uint8_t TITLE_LENGTH = 11;

// Start memory addresses for values
// in the Game Boy cartridge header
static constexpr uint16_t ENTRY_POINT_START_ADDRESS        = 0x100;
static constexpr uint16_t NINTENDO_LOGO_START_ADDRESS      = 0x104;
static constexpr uint16_t TITLE_START_ADDRESS              = 0x134;
static constexpr uint16_t MANUFACTURER_CODE_START_ADDRESS  = 0x13F;
static constexpr uint16_t CGB_FLAG_START_ADDRESS           = 0x143;
static constexpr uint16_t NEW_LICENSEE_CODE_START_ADDRESS  = 0x144;
static constexpr uint16_t SGB_FLAG_START_ADDRESS           = 0x146;
static constexpr uint16_t CARTRIDGE_TYPE_START_ADDRESS     = 0x147;
static constexpr uint16_t ROM_SIZE_START_ADDRESS           = 0x148;
static constexpr uint16_t RAM_SIZE_START_ADDRESS           = 0x149;
static constexpr uint16_t DESTINATION_CODE_START_ADDRESS   = 0x14A;
static constexpr uint16_t OLD_LICENSEE_CODE_START_ADDRESS  = 0x14B;
static constexpr uint16_t ROM_VERSION_NUMBER_START_ADDRESS = 0x14C;
static constexpr uint16_t HEADER_CHECKSUM_START_ADDRESS    = 0x14D;
static constexpr uint16_t GLOBAL_CHECKSUM_START_ADDRESS    = 0x14E;

enum class CartridgeType
{
    ROMOnly, MBC1, MBC2,
    MBC3, MBC4, MBC5,
    NotImplemented,
};

enum class ROMSize
{
    KB32, KB64, KB128,
    KB256, KB512, MB1,
    MB2, MB4, MB1p1,
    MB1p2, MB1p5,
};

enum class RAMSize
{
    None, KB2, KB8,
    KB32, KB64, KB128,
};

enum class Destination
{
    Japanese,
    NonJapanese,
};

class CartridgeInfo
{
    public:
        std::string title;

        // Cartridge info
        CartridgeType type;
        Destination destination;
        ROMSize romSize;
        RAMSize ramSize;
        std::string licenseCode;
        uint16_t version;

        uint16_t headerChecksum;
        uint16_t globalChecksum;

        // Whether the cartridge
        // supports Color Game Boy
        // or Super Game Boy
        // enhancements
        bool supportsCGB;
        bool supportsSGB;
};

CartridgeType getCartridgeType(uint8_t type);
std::string describeCartridge(CartridgeType type);
std::string getTitleFromCartridge(std::vector<uint8_t> &rom);

ROMSize getROMSize(uint8_t size);
std::string describeROM(ROMSize size);

RAMSize getRAMSize(uint8_t size);
uint32_t getActualRamSize(RAMSize size);
std::string describeRAM(RAMSize size);

Destination getDestination(uint8_t destination);
std::string describeDestination(Destination destination);

std::unique_ptr<CartridgeInfo> getInfo(const std::vector<uint8_t>& rom);
