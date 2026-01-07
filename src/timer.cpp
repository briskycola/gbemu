#include "timer.hpp"
#include "common.hpp"
#include "emulator.hpp"
#include "cpu.hpp"
#include <stdexcept>

Timer::Timer(Emulator &emulator) :
    emulator(emulator)
{}

uint8_t Timer::getDivider() const
{
    return divider.getValue();
}

uint8_t Timer::getTimer() const
{
    return timerCounter.getValue();
}

uint8_t Timer::getTimerModulo() const
{
    return timerModulo.getValue();
}

// Only the bottom three bits of the TAC register
// are usable.
uint8_t Timer::getTimerControl() const
{
    return timerControl.getValue() & 0x3;
}

void Timer::resetDivider()
{
    divider.set(0x0);
}

void Timer::setTimer(uint8_t value)
{
    timerCounter.set(value);
}

void Timer::setTimerModulo(uint8_t value)
{
    timerModulo.set(value);
}

void Timer::setTimerControl(uint8_t value)
{
    timerControl.set(value);
}

uint32_t Timer::clocksNeededToIncrement()
{
    switch (getTimerControl())
    {
        case 0: return GAMEBOY_CLOCK_RATE / 4096;
        case 1: return GAMEBOY_CLOCK_RATE / 262144;
        case 2: return GAMEBOY_CLOCK_RATE / 65536;
        case 3: return GAMEBOY_CLOCK_RATE / 16384;
        default: std::runtime_error("Invalid calculation in timer");
    }
    return 0x0;
}

void Timer::tick(uint32_t cycles)
{
    uint8_t newDivider = static_cast<uint8_t>(divider.getValue() + cycles);
    divider.set(newDivider);

    clocks += cycles * CLOCKS_PER_CYCLE;

    bool isTimerOn = timerControl.checkBit(2);
    if (isTimerOn == 0) return;

    uint32_t clockLimit = clocksNeededToIncrement();

    if (clocks >= clockLimit)
    {
        clocks = clocks % clockLimit;

        uint8_t oldTimerCounter = timerCounter.getValue();
        timerCounter.increment();

        if (timerCounter.getValue() < oldTimerCounter)
        {
            emulator.cpu.interruptFlag.setBitTo(2, true);
            timerCounter.set(timerModulo.getValue());
        }
    }
}
