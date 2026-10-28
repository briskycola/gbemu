#include "FrameBuffer.hpp"

FrameBuffer::FrameBuffer(uint32_t width, uint32_t height) :
    width(width),
    height(height),
    buffer(width * height, Color::White),
    rgbBuffer(width * height, shadeToRGB(Color::White))
{}

void FrameBuffer::setPixel(uint32_t x, uint32_t y, Color color)
{
    if (x >= width || y >= height) return;
    //buffer.at(pixelIndex(x, y)) = color;
    buffer[pixelIndex(x, y)] = color;
    rgbBuffer[pixelIndex(x, y)] = shadeToRGB(color);
}

void FrameBuffer::setPixelRGB(uint32_t x, uint32_t y, uint32_t rgb, uint8_t shade)
{
    if (x >= width || y >= height) return;
    buffer[pixelIndex(x, y)] = static_cast<Color>(shade & 3); // Keep the 2-bit shade for sprite priority
    rgbBuffer[pixelIndex(x, y)] = rgb & 0xFFFFFF; // Drop any alpha bits
}

Color FrameBuffer::getPixel(uint32_t x, uint32_t y) const
{
    return buffer.at(pixelIndex(x, y));
}

uint32_t FrameBuffer::getPixelRGB(uint32_t x, uint32_t y) const
{
    return rgbBuffer.at(pixelIndex(x, y));
}

uint32_t FrameBuffer::pixelIndex(uint32_t x, uint32_t y) const
{
    return (y * width) + x;
}

void FrameBuffer::reset()
{
    for (uint32_t i = 0; i < width * height; i++)
    {
        //buffer.at(i) = Color::White;
        buffer[i] = Color::White;
        rgbBuffer[i] = shadeToRGB(Color::White);
    }
}
