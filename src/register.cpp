#include "register.hpp"
#include "common.hpp"

void ByteRegister::set(const uint8_t newValue)
{
    value = newValue;
}

void ByteRegister::reset()
{
    value = 0x0;
}

uint8_t ByteRegister::getValue() const
{
    return value;
}

bool ByteRegister::checkBit(const uint8_t bit) const
{
    return ::checkBit(value, bit);
}

void ByteRegister::setBitTo(const uint8_t bit, const bool set)
{
    value = ::setBitTo(value, bit, set);
}

void ByteRegister::increment()
{
    value += 1;
}

void ByteRegister::decrement()
{
    value -= 1;
}

bool ByteRegister::operator==(uint8_t other)
{
    return value == other;
}

void WordRegister::set(const uint16_t newValue)
{
    value = newValue;
}

uint16_t WordRegister::getValue() const
{
    return value;
}

uint8_t WordRegister::getLowByte() const
{
    return static_cast<uint8_t>(value);
}

uint8_t WordRegister::getHighByte() const
{
    return static_cast<uint8_t>(value >> 8);
}

void WordRegister::increment()
{
    value += 1;
}

void WordRegister::decrement()
{
    value -= 1;
}

RegisterPair::RegisterPair(ByteRegister &highByte, ByteRegister &lowByte) :
    highByte(highByte),
    lowByte(lowByte)
{}

void RegisterPair::set(const uint16_t word)
{
    // Discard the upper byte
    lowByte.set(static_cast<uint8_t>(word));

    // Discard the lower byte
    highByte.set(static_cast<uint8_t>(word >> 8));
}

uint8_t RegisterPair::getLowByte() const
{
    return lowByte.getValue();
}

uint8_t RegisterPair::getHighByte() const
{
    return highByte.getValue();
}

uint16_t RegisterPair::getValue() const
{
    // Combines the two 8-bit values into a 16-bit value
    return ::composeBytes(highByte.getValue(), lowByte.getValue());
}

void RegisterPair::increment()
{
    set(getValue() + 1);
}

void RegisterPair::decrement()
{
    set(getValue() - 1);
}

void FlagRegister::set(const uint8_t newValue)
{
    // We do a bitwise mask with 0xF0
    // to get rid of the lower 4 bits.
    //
    // This is because the flags themselves
    // are stored only on the upper 8 bits.
    value = newValue & CPU_FLAG_MASK;
}

void FlagRegister::setZeroFlag(const bool set)
{
    setBitTo(ZERO_FLAG_BIT, set);
}

void FlagRegister::setSubtractionFlag(const bool set)
{
    setBitTo(SUBTRACTION_FLAG_BIT, set);
}

void FlagRegister::setHalfCarryFlag(const bool set)
{
    setBitTo(HALF_CARRY_FLAG_BIT, set);
}

void FlagRegister::setCarryFlag(const bool set)
{
    setBitTo(CARRY_FLAG_BIT, set);
}

bool FlagRegister::ZeroFlag() const
{
    return checkBit(ZERO_FLAG_BIT);
}

bool FlagRegister::SubtractionFlag() const
{
    return checkBit(SUBTRACTION_FLAG_BIT);
}

bool FlagRegister::HalfCarryFlag() const
{
    return checkBit(HALF_CARRY_FLAG_BIT);
}

bool FlagRegister::CarryFlag() const
{
    return checkBit(CARRY_FLAG_BIT);
}


uint8_t FlagRegister::getZeroFlagValue() const
{
    return static_cast<uint8_t>(ZeroFlag() ? 1 : 0);
}

uint8_t FlagRegister::getSubtractionFlagValue() const
{
    return static_cast<uint8_t>(SubtractionFlag() ? 1 : 0);
}

uint8_t FlagRegister::getHalfCarryFlagValue() const
{
    return static_cast<uint8_t>(HalfCarryFlag() ? 1 : 0);
}

uint8_t FlagRegister::getCarryFlagValue() const
{
    return static_cast<uint8_t>(CarryFlag() ? 1 : 0);
}
