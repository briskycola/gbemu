#pragma once

#include "Core/Common/Common.hpp"
#include "Core/Video/Color.hpp"
#include <vector>

class FrameBuffer
{
    private:
        uint32_t width;
        uint32_t height;
        std::vector<Color> buffer;
        std::vector<uint32_t> rgbBuffer;
        uint32_t pixelIndex(uint32_t x, uint32_t y) const;
    public:
        FrameBuffer(uint32_t width, uint32_t height);
        uint32_t getWidth() const { return width; }
        uint32_t getHeight() const { return height; }
        void setPixel(uint32_t x, uint32_t y, Color color);
        Color getPixel(uint32_t x, uint32_t y) const;
        uint32_t getPixelRGB(uint32_t x, uint32_t y) const;
        void setPixelRGB(uint32_t x, uint32_t y, uint32_t rgb, uint8_t shade);
        void reset();
};
