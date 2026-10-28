#include "Tile.hpp"
#include "Color.hpp"
#include "Common.hpp"

Tile::Tile(Address &address, MMU &mmu, uint32_t sizeMultiplier)
{
    // Set the whole framebuffer to be black
    for (uint32_t x = 0; x < TILE_WIDTH_PX; x++)
    {
        for (uint32_t y = 0; y < TILE_HEIGHT_PX * sizeMultiplier; y++)
        {
            //tileBuffer.at(pixelIndex(x, y)) = GBColor::Color0;
            tileBuffer[pixelIndex(x, y)] = GBColor::Color0;
        }
    }

    for (uint32_t tileLine = 0; tileLine < TILE_HEIGHT_PX * sizeMultiplier; tileLine++)
    {
        // 2 bytes per line of pixels * y lines
        uint32_t indexIntoTile = TILE_BYTES_PER_ROW * tileLine;
        Address lineStart = address + indexIntoTile;

        uint8_t pixelsOne = mmu.read(lineStart, true);
        uint8_t pixelsTwo = mmu.read(lineStart + 1, true);

        std::vector<uint8_t> pixelLine = getPixelLine(pixelsOne, pixelsTwo);

        for (uint32_t x = 0; x < TILE_WIDTH_PX; x++)
        {
            //tileBuffer.at(pixelIndex(x, tileLine)) = getColor(pixelLine.at(x));
            tileBuffer[pixelIndex(x, tileLine)] = getColor(pixelLine[x]);
        }
    }
}

uint32_t Tile::pixelIndex(uint32_t x, uint32_t y)
{
    return (y * TILE_HEIGHT_PX) + x;
}

GBColor Tile::getPixel(uint32_t x, uint32_t y)
{
    //return tileBuffer.at(pixelIndex(x, y));
    return tileBuffer[pixelIndex(x, y)];
}

std::vector<uint8_t> Tile::getPixelLine(uint8_t byteOne, uint8_t byteTwo)
{
    std::vector<uint8_t> pixelLine;

    for (uint8_t i = 0; i < TILE_WIDTH_PX; i++)
    {
        // Bit 7 holds the leftmost pixel. The second byte
        // supplies the high bit of each pixel's color number.
        const uint8_t bit = static_cast<uint8_t>((TILE_WIDTH_PX - 1) - i);
        uint8_t colorValue = composeBits(getBitValue(byteTwo, bit), getBitValue(byteOne, bit));
        pixelLine.push_back(colorValue);
    }

    return pixelLine;
}
