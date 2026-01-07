#pragma once

#include <cstdint>

enum class GBButton
{
    Up,
    Down,
    Left,
    Right,
    A,
    B,
    Select,
    Start,
};

class Input
{
    private:
        bool up = false;
        bool down = false;
        bool left = false;
        bool right = false;
        bool a = false;
        bool b = false;
        bool select = false;
        bool start = false;

        bool buttonSwitch = false;
        bool directionSwitch = false;

        void setButton(GBButton button, bool set);
    public:
        void buttonPressed(GBButton button);
        void buttonReleased(GBButton button);
        void write(uint8_t set);
        uint8_t getInput() const;
};
