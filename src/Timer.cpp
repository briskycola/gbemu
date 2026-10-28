#include "Timer.hpp"
#include "Common.hpp"
#include "Emulator.hpp"
#include "CPU.hpp"
#include <stdexcept>

Timer::Timer(Emulator &emulator) :
    emulator(emulator)
{}

uint8_t Timer::getDivider() const
{
    // DIV is the upper 8 bits of the internal 16-bit counter
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
    return timerControl.getValue() & TIMER_CONTROL_MASK;
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
    timerControl.set(value & TIMER_CONTROL_MASK);
    if (wasHigh && !timerInput()) incrementTimer();
}

bool Timer::timerInput() const
{
    if (!timerControl.checkBit(TIMER_ENABLE_BIT)) return false;

    // TIMA increments when this bit of the internal counter falls.
    // Indexed by TAC bits 0-1: 4096, 262144, 65536, and 16384 Hz.
    static constexpr uint8_t DIVIDER_BITS[4] = {9, 3, 5, 7};
    return (dividerCounter & (1u << DIVIDER_BITS[timerControl.getValue() & TIMER_CLOCK_SELECT_MASK])) != 0;
}

void Timer::incrementTimer()
{
    if (reloadDelay > 0) return;
    if (timerCounter.getValue() == 0xFF) // TIMA overflows
    {
        timerCounter.set(0);
        reloadDelay = TIMER_RELOAD_DELAY;
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
                emulator.cpu.interruptFlag.setBitTo(INTERRUPT_TIMER_BIT, true);
            }
        }

        if (wasHigh && !timerInput()) incrementTimer();
    }
}
