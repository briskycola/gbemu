#include "Core/Video/FrameBuffer.hpp"
#include <algorithm>

namespace
{
constexpr uint32_t RGB_MASK = 0xFFFFFF; // Drops any alpha bits
}

FrameBuffer::FrameBuffer(uint32_t width, uint32_t height) :
    width(width),
    height(height),
    shades(width * height, Shade::White),
    colors(width * height, shadeToRGB(Shade::White))
{}

void FrameBuffer::setPixel(uint32_t x, uint32_t y, Shade shade)
{
    setPixel(x, y, shade, shadeToRGB(shade));
}

void FrameBuffer::setPixel(uint32_t x, uint32_t y, Shade shade, uint32_t rgb)
{
    if (x >= width || y >= height) return;
    shades[pixelIndex(x, y)] = shade;
    colors[pixelIndex(x, y)] = rgb & RGB_MASK;
}

void FrameBuffer::setPixelRGB(uint32_t x, uint32_t y, uint32_t rgb)
{
    setPixel(x, y, Shade::White, rgb);
}

Shade FrameBuffer::getShade(uint32_t x, uint32_t y) const
{
    return shades.at(pixelIndex(x, y));
}

uint32_t FrameBuffer::getPixelRGB(uint32_t x, uint32_t y) const
{
    return colors.at(pixelIndex(x, y));
}

uint32_t FrameBuffer::pixelIndex(uint32_t x, uint32_t y) const
{
    return (y * width) + x;
}

void FrameBuffer::reset()
{
    std::fill(shades.begin(), shades.end(), Shade::White);
    std::fill(colors.begin(), colors.end(), shadeToRGB(Shade::White));
}
