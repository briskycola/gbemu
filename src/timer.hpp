#pragma once

#include "register.hpp"

// https://gbdev.io/pandocs/Timer_and_Divider_Registers.html

static constexpr uint8_t CLOCKS_PER_CYCLE = 4;

class Emulator;

class Timer
{
    private:
        uint32_t clocks = 0;
        Emulator &emulator;

        ByteRegister divider;
        ByteRegister timerCounter;
        ByteRegister timerModulo;
        ByteRegister timerControl;

        uint32_t clocksNeededToIncrement();
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
