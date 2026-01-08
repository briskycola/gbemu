#include "tile.hpp"
#include "color.hpp"
#include "common.hpp"

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
        uint32_t indexIntoTile = 2 * tileLine;
        Address lineStart = address + indexIntoTile;

        uint8_t pixelsOne = mmu.read(lineStart);
        uint8_t pixelsTwo = mmu.read(lineStart + 1);

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

    for (uint8_t i = 0; i < 8; i++)
    {
        uint8_t colorValue = static_cast<uint8_t>((getBitValue(byteTwo, 7 - i) << 1) | getBitValue(byteOne, 7 - i));
        pixelLine.push_back(colorValue);
    }

    return pixelLine;
}
