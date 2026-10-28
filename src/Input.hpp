#pragma once

#include <cstdint>

// JOYP (0xFF00) layout. Writing 0 to bit 4 selects the
// d-pad and writing 0 to bit 5 selects the buttons; bits
// 0-3 then read 0 for each pressed key. Bits 6-7 are unused.
static constexpr uint8_t JOYPAD_SELECT_DIRECTION_BIT = 4;
static constexpr uint8_t JOYPAD_SELECT_BUTTONS_BIT   = 5;
static constexpr uint8_t JOYPAD_SELECT_MASK         = 0x30;
static constexpr uint8_t JOYPAD_KEYS_MASK           = 0x0F;
static constexpr uint8_t JOYPAD_UNUSED_BITS         = 0xC0;

// Key bits, shared between the d-pad and the buttons
static constexpr uint8_t JOYPAD_RIGHT_OR_A_BIT    = 0;
static constexpr uint8_t JOYPAD_LEFT_OR_B_BIT     = 1;
static constexpr uint8_t JOYPAD_UP_OR_SELECT_BIT  = 2;
static constexpr uint8_t JOYPAD_DOWN_OR_START_BIT = 3;

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
