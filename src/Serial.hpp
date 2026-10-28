#pragma once

#include <cstdint>

// https://gbdev.io/pandocs/Serial_Data_Transfer_(Link_Cable).html

class Serial
{
    private:
        uint8_t data;
    public:
        Serial();

        uint8_t read() const;
        void write(uint8_t byte);
        void writeControl(uint8_t byte) const;
};
