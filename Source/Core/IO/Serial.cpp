#include "Core/IO/Serial.hpp"
#include "Core/Common/Common.hpp"
#include "Core/Emulator.hpp"
#include <iostream>

Serial::Serial(Emulator &emulator) :
    emulator(emulator)
{}

// https://gbdev.io/pandocs/Serial_Data_Transfer_(Link_Cable).html#ff01--sb-serial-transfer-data
uint8_t Serial::read() const
{
    return data;
}

void Serial::write(const uint8_t byte)
{
    data = byte;
}

uint8_t Serial::readControl() const
{
    const uint8_t used = emulator.cgbMode ? io::SERIAL_CONTROL_BITS_CGB : io::SERIAL_CONTROL_BITS;
    return static_cast<uint8_t>(~used | (control & used));
}

void Serial::writeControl(const uint8_t byte)
{
    control = byte & (emulator.cgbMode ? io::SERIAL_CONTROL_BITS_CGB : io::SERIAL_CONTROL_BITS);

    // Setting bit 7 starts a transfer of SB; clearing it stops one
    bitsRemaining = checkBit(control, io::SERIAL_TRANSFER_START_BIT) ? io::SERIAL_BITS_PER_TRANSFER : 0;

    // Test ROMs (such as Blargg's) report their results by sending text over
    // the link port, so show each byte sent on the console
    if (bitsRemaining)
    {
        std::cout << data;
        std::cout.flush();
    }
}

bool Serial::internalClock() const
{
    return checkBit(control, io::SERIAL_CLOCK_SELECT_BIT);
}

uint8_t Serial::clockCounterBit() const
{
    const bool fast = emulator.cgbMode && checkBit(control, io::SERIAL_CLOCK_SPEED_BIT);
    return fast ? io::SERIAL_FAST_CLOCK_COUNTER_BIT : io::SERIAL_CLOCK_COUNTER_BIT;
}

// Each internal clock shifts SB left by one bit. With no cable, the bit
// shifted in is 1, so a transfer receives $FF. After 8 bits SC bit 7 clears
// and the serial interrupt is requested. An externally clocked transfer has
// no other Game Boy to clock it, so it never finishes.
// https://gbdev.io/pandocs/Serial_Data_Transfer_(Link_Cable).html#disconnects
void Serial::clock()
{
    if (!bitsRemaining || !internalClock()) return;
    data = static_cast<uint8_t>(data << 1 | 1);
    if (--bitsRemaining) return;
    control = setBitTo(control, io::SERIAL_TRANSFER_START_BIT, false);
    emulator.cpu.interruptFlag.setBitTo(cpu::INTERRUPT_SERIAL_BIT, true);
}
