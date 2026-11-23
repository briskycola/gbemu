#include "Input.hpp"
#include "Common.hpp"
#include "Emulator.hpp"

Input::Input(Emulator &emulator) :
    emulator(emulator)
{}

void Input::buttonPressed(GBButton button)
{
    const uint8_t before = getInput();
    setButton(button, true);
    checkInterrupt(before);
}

void Input::buttonReleased(GBButton button)
{
    setButton(button, false);
}

// Selecting a group while one of its keys is held also pulls a line low
void Input::write(uint8_t set)
{
    const uint8_t before = getInput();
    directionSwitch = !checkBit(set, input::JOYPAD_SELECT_DIRECTION_BIT);
    buttonSwitch = !checkBit(set, input::JOYPAD_SELECT_BUTTONS_BIT);
    checkInterrupt(before);
}

// The joypad interrupt is requested when any of JOYP bits 0-3 goes from
// high to low
// https://gbdev.io/pandocs/Interrupt_Sources.html#int-60--joypad-interrupt
void Input::checkInterrupt(uint8_t before)
{
    const uint8_t fell = before & static_cast<uint8_t>(~getInput()) & input::JOYPAD_KEYS_MASK;
    if (fell) emulator.cpu.interruptFlag.setBitTo(cpu::INTERRUPT_JOYPAD_BIT, true);
}

uint8_t Input::getInput() const
{
    // Keys read 1 when released
    uint8_t directionButtons = input::JOYPAD_KEYS_MASK;
    uint8_t actionButtons = input::JOYPAD_KEYS_MASK;

    if (directionSwitch)
    {
        directionButtons = setBitTo(directionButtons, input::JOYPAD_RIGHT_OR_A_BIT, !right);
        directionButtons = setBitTo(directionButtons, input::JOYPAD_LEFT_OR_B_BIT, !left);
        directionButtons = setBitTo(directionButtons, input::JOYPAD_UP_OR_SELECT_BIT, !up);
        directionButtons = setBitTo(directionButtons, input::JOYPAD_DOWN_OR_START_BIT, !down);
    }

    if (buttonSwitch)
    {
        actionButtons = setBitTo(actionButtons, input::JOYPAD_RIGHT_OR_A_BIT, !a);
        actionButtons = setBitTo(actionButtons, input::JOYPAD_LEFT_OR_B_BIT, !b);
        actionButtons = setBitTo(actionButtons, input::JOYPAD_UP_OR_SELECT_BIT, !select);
        actionButtons = setBitTo(actionButtons, input::JOYPAD_DOWN_OR_START_BIT, !start);
    }

    uint8_t buttons = directionSwitch && buttonSwitch
        ? directionButtons & actionButtons
        : (directionSwitch ? directionButtons : actionButtons);
    buttons = setBitTo(buttons, input::JOYPAD_SELECT_DIRECTION_BIT, !directionSwitch);
    buttons = setBitTo(buttons, input::JOYPAD_SELECT_BUTTONS_BIT, !buttonSwitch);

    return buttons | input::JOYPAD_UNUSED_BITS;
}

void Input::setButton(GBButton button, bool set)
{
    if (button == GBButton::Up) { up = set; }
    if (button == GBButton::Down) { down = set; }
    if (button == GBButton::Left) { left = set; }
    if (button == GBButton::Right) { right = set; }
    if (button == GBButton::A) { a = set; }
    if (button == GBButton::B) { b = set; }
    if (button == GBButton::Select) { select = set; }
    if (button == GBButton::Start) { start = set; }
}
