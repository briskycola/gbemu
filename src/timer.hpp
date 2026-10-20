#pragma once

#include "register.hpp"
#include <cstdint>

// https://gbdev.io/pandocs/Timer_and_Divider_Registers.html

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
