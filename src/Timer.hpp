#pragma once

#include "Register.hpp"
#include <cstdint>

// https://gbdev.io/pandocs/Timer_and_Divider_Registers.html

// TAC: bit 2 enables the timer and bits 0-1 pick its speed.
// The other bits are unused.
static constexpr uint8_t TIMER_CONTROL_MASK     = 0x07;
static constexpr uint8_t TIMER_ENABLE_BIT       = 2;
static constexpr uint8_t TIMER_CLOCK_SELECT_MASK = 0x03;

// After TIMA overflows it reads 0 for 4 clocks
// before being reloaded from TMA
static constexpr uint8_t TIMER_RELOAD_DELAY = 4;

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

        bool timerInput() const;
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
};
