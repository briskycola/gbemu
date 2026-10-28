#include "input.hpp"
#include "common.hpp"

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
    directionSwitch = !checkBit(set, 4);
    buttonSwitch = !checkBit(set, 5);
}

uint8_t Input::getInput() const
{
    uint8_t directionButtons = 0b1111;
    uint8_t actionButtons = 0b1111;

    if (directionSwitch)
    {
        directionButtons = setBitTo(directionButtons, 0, !right);
        directionButtons = setBitTo(directionButtons, 1, !left);
        directionButtons = setBitTo(directionButtons, 2, !up);
        directionButtons = setBitTo(directionButtons, 3, !down);
    }

    if (buttonSwitch)
    {
        actionButtons = setBitTo(actionButtons, 0, !a);
        actionButtons = setBitTo(actionButtons, 1, !b);
        actionButtons = setBitTo(actionButtons, 2, !select);
        actionButtons = setBitTo(actionButtons, 3, !start);
    }

    uint8_t buttons = directionSwitch && buttonSwitch
        ? directionButtons & actionButtons
        : (directionSwitch ? directionButtons : actionButtons);
    buttons = setBitTo(buttons, 4, !directionSwitch);
    buttons = setBitTo(buttons, 5, !buttonSwitch);

    return buttons | 0b1100'0000;
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
