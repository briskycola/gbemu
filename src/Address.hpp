#pragma once

#include "Register.hpp"

class Address
{
    private:
        uint16_t address;
    public:
        Address(uint16_t location);
        explicit Address(const RegisterPair &from);
        explicit Address(const WordRegister &from);

        uint16_t getValue() const;
        bool inRange(Address low, Address high) const;

        bool operator == (uint16_t other) const;
        Address operator + (uint16_t other) const;
        Address operator - (uint16_t other) const;
};
