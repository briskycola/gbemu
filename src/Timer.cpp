#include "Timer.hpp"
#include "Common.hpp"
#include "Emulator.hpp"
#include "CPU.hpp"
#include <algorithm>
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
    return timerControl.getValue() & timer::TIMER_CONTROL_MASK;
}

// Writing DIV resets the whole system counter, which can make the selected
// bits fall and so tick TIMA or the APU's frame sequencer early
// https://gbdev.io/pandocs/Timer_Obscure_Behaviour.html#relation-between-timer-and-divider-register
void Timer::resetDivider()
{
    const bool wasHigh = selectedBitHigh();
    const bool serialWasHigh = serialClockHigh();
    const bool frameSequencerWasHigh = isFrameSequencerInputHigh();
    dividerCounter = 0;
    tickOnFallingEdge(wasHigh, isTimerEnabled());
    if (frameSequencerWasHigh) emulator.apu.clockFrameSequencerFromDivider();
    if (serialWasHigh) emulator.serial.clock();
}

// https://gbdev.io/pandocs/Timer_Obscure_Behaviour.html#timer-overflow-behavior
void Timer::setTimer(uint8_t value)
{
    // In the cycle TMA is copied in (cycle B) the write is lost
    if (reloadedThisCycle) return;

    // In the cycle after an overflow (cycle A) the write cancels the reload
    // and the interrupt
    timerCounter.set(value);
    reloadDelay = 0;
}

void Timer::setTimerModulo(uint8_t value)
{
    timerModulo.set(value);

    // A TMA write in the cycle TMA is copied in (cycle B) is copied to TIMA too.
    // (The Timer and Divider Registers page says the old value is copied, but
    // the more detailed timer obscure behavior page and hardware tests agree
    // on the new one.)
    if (reloadedThisCycle) timerCounter.set(value);
}

// https://gbdev.io/pandocs/Timer_Obscure_Behaviour.html#relation-between-timer-and-divider-register
void Timer::setTimerControl(uint8_t value)
{
    const bool wasHigh = selectedBitHigh();
    const bool wasEnabled = isTimerEnabled();
    timerControl.set(value & timer::TIMER_CONTROL_MASK);

    if (emulator.cgbHardware)
    {
        // The CGB ANDs the enable bit after the falling edge detector, so
        // only switching from a set bit to a clear one ticks, while enabled
        if (wasEnabled && isTimerEnabled() && wasHigh && !selectedBitHigh()) incrementTimer();
    }
    else
    {
        // The DMG ANDs it before the detector, so disabling the timer while
        // the selected bit is set also ticks once
        if (wasEnabled && wasHigh && !(isTimerEnabled() && selectedBitHigh())) incrementTimer();
    }
}

bool Timer::isFrameSequencerInputHigh() const
{
    const uint8_t bit = emulator.doubleSpeed ? timer::FRAME_SEQUENCER_BIT_DOUBLE_SPEED : timer::FRAME_SEQUENCER_BIT;
    return (dividerCounter & (1u << bit)) != 0;
}

// The serial port's internal clock is driven by a bit of the system counter too
// https://gbdev.io/pandocs/Serial_Data_Transfer_(Link_Cable).html#internal-clock
bool Timer::serialClockHigh() const
{
    return (dividerCounter & (1u << emulator.serial.clockCounterBit())) != 0;
}

bool Timer::isTimerEnabled() const
{
    return timerControl.checkBit(timer::TIMER_ENABLE_BIT);
}

// The system counter bit TAC's clock select picks. TIMA ticks when it falls.
bool Timer::selectedBitHigh() const
{
    // Indexed by TAC bits 0-1: every 256, 4, 16 and 64 M-cycles
    // https://gbdev.io/pandocs/Timer_and_Divider_Registers.html#ff07--tac-timer-control
    static constexpr uint8_t DIVIDER_BITS[4] = {9, 3, 5, 7};
    return (dividerCounter & (1u << DIVIDER_BITS[timerControl.getValue() & timer::TIMER_CLOCK_SELECT_MASK])) != 0;
}

// Ticks TIMA if the selected bit fell while the timer is enabled
void Timer::tickOnFallingEdge(bool wasHigh, bool enabled)
{
    if (enabled && wasHigh && !selectedBitHigh()) incrementTimer();
}

// https://gbdev.io/pandocs/Timer_Obscure_Behaviour.html#timer-overflow-behavior
void Timer::incrementTimer()
{
    if (reloadDelay > 0) return;
    if (timerCounter.getValue() == 0xFF)
    {
        // TIMA reads $00 for one M-cycle before TMA is copied in and the
        // interrupt is requested
        timerCounter.set(0);
        reloadDelay = timer::TIMER_RELOAD_DELAY;
    }
    else
    {
        timerCounter.increment();
    }
}

// One clock of the system counter. TIMA and the APU's frame sequencer are
// driven by falling edges of its bits.
// https://gbdev.io/pandocs/Timer_Obscure_Behaviour.html
void Timer::step()
{
    const bool wasHigh = selectedBitHigh();
    const bool serialWasHigh = serialClockHigh();
    const bool frameSequencerWasHigh = isFrameSequencerInputHigh();
    ++dividerCounter;
    if (frameSequencerWasHigh && !isFrameSequencerInputHigh()) emulator.apu.clockFrameSequencerFromDivider();
    if (serialWasHigh && !serialClockHigh()) emulator.serial.clock();

    if (reloadDelay > 0 && --reloadDelay == 0)
    {
        timerCounter.set(timerModulo.getValue());
        emulator.cpu.interruptFlag.setBitTo(cpu::INTERRUPT_TIMER_BIT, true);
        reloadedThisCycle = true;
    }

    tickOnFallingEdge(wasHigh, isTimerEnabled());
}

void Timer::tick(uint32_t cycles)
{
    // Skip the clocks tickAhead() already ran for this M-cycle
    const uint32_t skipped = std::min(clocksAhead, cycles);
    clocksAhead = 0;

    // A new M-cycle starts here unless tickAhead() already started it. The
    // reload is only special for writes in the cycle it happened in.
    if (skipped == 0) reloadedThisCycle = false;

    // Step per clock so writes and overflows keep their order
    for (uint32_t cycle = skipped; cycle < cycles; ++cycle) step();
}

void Timer::tickAhead(uint32_t cycles)
{
    reloadedThisCycle = false;
    for (uint32_t cycle = 0; cycle < cycles; ++cycle) step();
    clocksAhead = cycles;
}
