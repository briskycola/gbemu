#pragma once

#include "address.hpp"
#include "common.hpp"
#include "mmu.hpp"
#include <array>

static constexpr uint32_t TILES_PER_LINE  = 32;
static constexpr uint32_t TILE_HEIGHT_PX  = 8;
static constexpr uint32_t TILE_WIDTH_PX   = 8;

static const Address TILE_SET_ZERO_ADDRESS = 0x8000;
static const Address TILE_SET_ONE_ADDRESS  = 0x8800;

static const Address TILE_MAP_ZERO_ADDRESS = 0x9800;
static const Address TILE_MAP_ONE_ADDRESS  = 0x9C00;

// A single tile contains 8 lines, each are two bytes
static constexpr uint32_t TILE_BYTES   = 2 * 8;
static constexpr uint32_t SPRITE_BYTES = 4;

class Tile
{
    private:
        std::array<GBColor, TILE_HEIGHT_PX * 2 * TILE_WIDTH_PX> tileBuffer;

        uint32_t pixelIndex(uint32_t x, uint32_t y);
        std::vector<uint8_t> getPixelLine(uint8_t byteOne, uint8_t byteTwo);
    public:
        Tile(Address &address, MMU &mmu, uint32_t sizeMultiplier = 1);
        GBColor getPixel(uint32_t x, uint32_t y);
};
