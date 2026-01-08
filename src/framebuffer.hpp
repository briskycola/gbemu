#pragma once

#include "common.hpp"
#include <vector>

class FrameBuffer
{
    private:
        uint32_t width;
        uint32_t height;
        std::vector<Color> buffer;
        uint32_t pixelIndex(uint32_t x, uint32_t y) const;
    public:
        FrameBuffer(uint32_t width, uint32_t height);
        void setPixel(uint32_t x, uint32_t y, Color color);
        Color getPixel(uint32_t x, uint32_t y) const;
        void reset();
};
