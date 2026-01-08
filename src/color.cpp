#include "color.hpp"
#include <stdexcept>

GBColor getColor(uint8_t pixelValue)
{
    switch (pixelValue)
    {
        case 0: return GBColor::Color0;
        case 1: return GBColor::Color1;
        case 2: return GBColor::Color2;
        case 3: return GBColor::Color3;
        default: std::runtime_error("Invalid color code");
    }
    //return GBColor::Color0;
    return {};
}
