#include "Input.hpp"
#include "Common.hpp"

void Input::buttonPressed(GBButton button)
{
    setButton(button, true);
}

void Input::buttonReleased(GBButton button)
{
    setButton(button, false);
}

void Input::write(uint8_t set)
{
    directionSwitch = !checkBit(set, JOYPAD_SELECT_DIRECTION_BIT);
    buttonSwitch = !checkBit(set, JOYPAD_SELECT_BUTTONS_BIT);
}

uint8_t Input::getInput() const
{
    // Keys read 1 when released
    uint8_t directionButtons = JOYPAD_KEYS_MASK;
    uint8_t actionButtons = JOYPAD_KEYS_MASK;

    if (directionSwitch)
    {
        directionButtons = setBitTo(directionButtons, JOYPAD_RIGHT_OR_A_BIT, !right);
        directionButtons = setBitTo(directionButtons, JOYPAD_LEFT_OR_B_BIT, !left);
        directionButtons = setBitTo(directionButtons, JOYPAD_UP_OR_SELECT_BIT, !up);
        directionButtons = setBitTo(directionButtons, JOYPAD_DOWN_OR_START_BIT, !down);
    }

    if (buttonSwitch)
    {
        actionButtons = setBitTo(actionButtons, JOYPAD_RIGHT_OR_A_BIT, !a);
        actionButtons = setBitTo(actionButtons, JOYPAD_LEFT_OR_B_BIT, !b);
        actionButtons = setBitTo(actionButtons, JOYPAD_UP_OR_SELECT_BIT, !select);
        actionButtons = setBitTo(actionButtons, JOYPAD_DOWN_OR_START_BIT, !start);
    }

    uint8_t buttons = directionSwitch && buttonSwitch
        ? directionButtons & actionButtons
        : (directionSwitch ? directionButtons : actionButtons);
    buttons = setBitTo(buttons, JOYPAD_SELECT_DIRECTION_BIT, !directionSwitch);
    buttons = setBitTo(buttons, JOYPAD_SELECT_BUTTONS_BIT, !buttonSwitch);

    return buttons | JOYPAD_UNUSED_BITS;
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
