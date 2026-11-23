#pragma once

#include "Register.hpp"
#include <cstdint>

// https://gbdev.io/pandocs/Timer_and_Divider_Registers.html

// TAC: bit 2 enables the timer and bits 0-1 pick its speed.
// The other bits are unused.
namespace timer
{
    static constexpr uint8_t TIMER_CONTROL_MASK      = 0x07;
    static constexpr uint8_t TIMER_ENABLE_BIT        = 2;
    static constexpr uint8_t TIMER_CLOCK_SELECT_MASK = 0x03;
    
    // The APU frame sequencer (512 Hz) is clocked when this bit of the
    // internal counter falls. In double speed the counter runs twice as
    // fast, so the next bit up keeps it at 512 Hz.
    static constexpr uint8_t FRAME_SEQUENCER_BIT              = 12;
    static constexpr uint8_t FRAME_SEQUENCER_BIT_DOUBLE_SPEED = 13;

    // After TIMA overflows it reads 0 for 4 clocks
    // before being reloaded from TMA
    static constexpr uint8_t TIMER_RELOAD_DELAY = 4;

    // Clocks into its M-cycle at which a TAC write takes effect
    static constexpr uint32_t TAC_WRITE_CLOCKS = 2;
}

class Emulator;

class Timer
{
    private:
        Emulator &emulator;

        uint16_t dividerCounter = 0;
        ByteRegister timerCounter;
        ByteRegister timerModulo;
        ByteRegister timerControl;
        uint8_t reloadDelay = 0;
        bool reloadedThisCycle = false; // TMA was copied to TIMA in the current M-cycle
        uint32_t clocksAhead = 0;       // Clocks of the coming M-cycle already run by tickAhead()

        void step();

        bool isTimerEnabled() const;
        bool selectedBitHigh() const;
        void tickOnFallingEdge(bool wasHigh, bool enabled);
        bool isFrameSequencerInputHigh() const;
        void incrementTimer();
    public:
        Timer(Emulator &emulator);

        uint8_t getDivider() const;
        uint8_t getTimer() const;
        uint8_t getTimerModulo() const;
        uint8_t getTimerControl() const;

        void resetDivider();
        void setTimer(uint8_t value);
        void setTimerModulo(uint8_t value);
        void setTimerControl(uint8_t value);

        void tick(uint32_t cycles);

        // Runs the first clocks of the coming M-cycle now, for writes that
        // take effect partway through it. The next tick() skips them.
        void tickAhead(uint32_t cycles);
};
