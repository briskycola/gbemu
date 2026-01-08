#include "framebuffer.hpp"

FrameBuffer::FrameBuffer(uint32_t width, uint32_t height) :
    width(width),
    height(height),
    buffer(width * height, Color::White)
{}

void FrameBuffer::setPixel(uint32_t x, uint32_t y, Color color)
{
    if (x >= width || y >= height) return;
    //buffer.at(pixelIndex(x, y)) = color;
    buffer[pixelIndex(x, y)] = color;
}

Color FrameBuffer::getPixel(uint32_t x, uint32_t y) const
{
    return buffer.at(pixelIndex(x, y));
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
    }
}
