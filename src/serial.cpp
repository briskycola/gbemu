#include "serial.hpp"
#include "common.hpp"
#include <iostream>

Serial::Serial() {}

uint8_t Serial::read() const
{
    return data;
}

void Serial::write(const uint8_t byte)
{
    data = byte;
}

void Serial::writeControl(const uint8_t byte) const
{
    if (checkBit(byte, 7))
    {
        std::cout << data;
        std::cout.flush();
    }
}
