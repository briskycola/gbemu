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
    uint8_t buttons = 0b1111;

    if (directionSwitch)
    {
        buttons = setBitTo(buttons, 0, !right);
        buttons = setBitTo(buttons, 1, !left);
        buttons = setBitTo(buttons, 2, !up);
        buttons = setBitTo(buttons, 3, !down);
    }

    if (buttonSwitch)
    {
        buttons = setBitTo(buttons, 0, !a);
        buttons = setBitTo(buttons, 1, !b);
        buttons = setBitTo(buttons, 2, !select);
        buttons = setBitTo(buttons, 3, !start);
    }

    buttons = setBitTo(buttons, 4, !directionSwitch);
    buttons = setBitTo(buttons, 5, !buttonSwitch);

    return buttons;
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
