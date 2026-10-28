#pragma once

#include <cstdint>

// https://gbdev.io/pandocs/Serial_Data_Transfer_(Link_Cable).html

// Setting bit 7 of SC starts a transfer of the byte in SB
static constexpr uint8_t SERIAL_TRANSFER_START_BIT = 7;

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
