#pragma once

#include <cstdint>

// https://gbdev.io/pandocs/Serial_Data_Transfer_(Link_Cable).html

namespace io
{
    // SC: bit 7 requests or reports a transfer, bit 1 picks the CGB's fast
    // clock and bit 0 picks the internal clock. The other bits read as 1
    // (bit 1 too, outside CGB mode).
    // https://gbdev.io/pandocs/Serial_Data_Transfer_(Link_Cable).html#ff02--sc-serial-transfer-control
    static constexpr uint8_t SERIAL_TRANSFER_START_BIT = 7;
    static constexpr uint8_t SERIAL_CLOCK_SPEED_BIT    = 1;
    static constexpr uint8_t SERIAL_CLOCK_SELECT_BIT   = 0;
    static constexpr uint8_t SERIAL_CONTROL_BITS       = 0x81;
    static constexpr uint8_t SERIAL_CONTROL_BITS_CGB   = 0x83;

    // The internal clock shifts one bit each time this system counter bit
    // falls: 8192 Hz, or 262144 Hz with the CGB's fast clock. Double speed
    // runs the counter, and so the clock, twice as fast.
    // https://gbdev.io/pandocs/Serial_Data_Transfer_(Link_Cable).html#internal-clock
    static constexpr uint8_t SERIAL_CLOCK_COUNTER_BIT      = 8;
    static constexpr uint8_t SERIAL_FAST_CLOCK_COUNTER_BIT = 3;

    static constexpr uint8_t SERIAL_BITS_PER_TRANSFER = 8;
}

class Emulator;

class Serial
{
    private:
        Emulator &emulator;
        uint8_t data = 0;         // SB
        uint8_t control = 0;      // SC
        uint8_t bitsRemaining = 0;

        bool internalClock() const;

    public:
        Serial(Emulator &emulator);

        uint8_t read() const;
        void write(uint8_t byte);
        uint8_t readControl() const;
        void writeControl(uint8_t byte);

        // The system counter bit whose falling edge clocks the next bit
        uint8_t clockCounterBit() const;

        // Called by the Timer on each falling edge of that bit
        void clock();
};
