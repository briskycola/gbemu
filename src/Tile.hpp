#pragma once

#include "Address.hpp"
#include "Common.hpp"
#include "Color.hpp"
#include "MMU.hpp"
#include <array>

// Tile maps are 32x32 tiles of 8x8 pixels
static constexpr uint32_t TILES_PER_LINE = 32;
static constexpr uint32_t TILE_HEIGHT_PX = 8;
static constexpr uint32_t TILE_WIDTH_PX  = 8;

// The two tile data areas. Set zero uses unsigned tile
// IDs from 0x8000; set one uses signed IDs centered on 0x9000.
static const Address TILE_SET_ZERO_ADDRESS = 0x8000;
static const Address TILE_SET_ONE_ADDRESS  = 0x8800;

// The two 32x32 tile maps
static const Address TILE_MAP_ZERO_ADDRESS = 0x9800;
static const Address TILE_MAP_ONE_ADDRESS  = 0x9C00;

// A single tile contains 8 lines, each are two bytes
static constexpr uint32_t TILE_BYTES_PER_ROW = 2;
static constexpr uint32_t TILE_BYTES       = TILE_BYTES_PER_ROW * TILE_HEIGHT_PX;

// Each OAM sprite entry is 4 bytes
static constexpr uint32_t SPRITE_BYTES = 4;

// Tall sprites (8x16) are two tiles stacked vertically
static constexpr uint32_t MAX_SPRITE_HEIGHT_IN_TILES = 2;

class Tile
{
    private:
        std::array<GBColor, TILE_HEIGHT_PX * MAX_SPRITE_HEIGHT_IN_TILES * TILE_WIDTH_PX> tileBuffer;

        uint32_t pixelIndex(uint32_t x, uint32_t y);
        std::vector<uint8_t> getPixelLine(uint8_t byteOne, uint8_t byteTwo);
    public:
        Tile(Address &address, MMU &mmu, uint32_t sizeMultiplier = 1);
        GBColor getPixel(uint32_t x, uint32_t y);
};
