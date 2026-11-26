#pragma once

#include "Core/Common/Common.hpp"
#include "Core/Video/Color.hpp"
#include <vector>

// A picture: each pixel's 24-bit RGB color, and the gray shade it shows.
// On the original Game Boy the color is the shade's gray. On the Super Game
// Boy, and on the Game Boy Color running a Game Boy game, the shade picks a
// color from a palette, and the shade is kept for the Super Game Boy's VRAM
// transfers and for recoloring. In CGB Mode there are no shades, so every
// pixel's shade is White.
class FrameBuffer
{
    private:
        uint32_t width;
        uint32_t height;
        std::vector<Shade> shades;
        std::vector<uint32_t> colors;
        uint32_t pixelIndex(uint32_t x, uint32_t y) const;
    public:
        FrameBuffer(uint32_t width, uint32_t height);
        uint32_t getWidth() const { return width; }
        uint32_t getHeight() const { return height; }

        // A shade shown as its gray
        void setPixel(uint32_t x, uint32_t y, Shade shade);
        // A shade shown in a palette's color
        void setPixel(uint32_t x, uint32_t y, Shade shade, uint32_t rgb);
        // A color without a shade (CGB Mode)
        void setPixelRGB(uint32_t x, uint32_t y, uint32_t rgb);

        Shade getShade(uint32_t x, uint32_t y) const;
        uint32_t getPixelRGB(uint32_t x, uint32_t y) const;
        void reset();
};
