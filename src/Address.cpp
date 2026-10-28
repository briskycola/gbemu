#include "address.hpp"

Address::Address(uint16_t location) :
    address(location)
{}

Address::Address(const RegisterPair &from) :
    address(from.getValue())
{}

Address::Address(const WordRegister &from) :
    address(from.getValue())
{}

uint16_t Address::getValue() const
{
    return address;
}

bool Address::inRange(Address low, Address high) const
{
    return low.getValue() <= getValue() && getValue() <= high.getValue();
}

bool Address::operator == (uint16_t other) const
{
    return address == other;
}

Address Address::operator + (uint16_t other) const
{
    return Address{static_cast<uint16_t>(address + other)};
}

Address Address::operator - (uint16_t other) const
{
    return Address{static_cast<uint16_t>(address - other)};
}
