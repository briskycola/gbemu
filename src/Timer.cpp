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
    return static_cast<uint8_t>(dividerCounter >> 8);
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
    return timerControl.getValue() & 0x07;
}

void Timer::resetDivider()
{
    const bool wasHigh = timerInput();
    dividerCounter = 0;
    if (wasHigh && !timerInput()) incrementTimer();
}

void Timer::setTimer(uint8_t value)
{
    timerCounter.set(value);
    if (reloadDelay > 0) reloadDelay = 0;
}

void Timer::setTimerModulo(uint8_t value)
{
    timerModulo.set(value);
}

void Timer::setTimerControl(uint8_t value)
{
    const bool wasHigh = timerInput();
    timerControl.set(value & 0x07);
    if (wasHigh && !timerInput()) incrementTimer();
}

bool Timer::timerInput() const
{
    if (!timerControl.checkBit(2)) return false;
    static constexpr uint8_t dividerBits[4] = {9, 3, 5, 7};
    return (dividerCounter & (1u << dividerBits[timerControl.getValue() & 0x03])) != 0;
}

void Timer::incrementTimer()
{
    if (reloadDelay > 0) return;
    if (timerCounter.getValue() == 0xFF)
    {
        timerCounter.set(0);
        reloadDelay = 4;
    }
    else
    {
        timerCounter.increment();
    }
}

void Timer::tick(uint32_t cycles)
{
    // Timer and divider inputs are driven by falling edges of the internal
    // divider counter. Step per T-cycle so writes and overflows retain order.
    for (uint32_t cycle = 0; cycle < cycles; ++cycle)
    {
        const bool wasHigh = timerInput();
        ++dividerCounter;

        if (reloadDelay > 0)
        {
            --reloadDelay;
            if (reloadDelay == 0)
            {
                timerCounter.set(timerModulo.getValue());
                emulator.cpu.interruptFlag.setBitTo(2, true);
            }
        }

        if (wasHigh && !timerInput()) incrementTimer();
    }
}
