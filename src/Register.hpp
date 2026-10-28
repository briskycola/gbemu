#pragma once

#include "Common.hpp"

// In the Game Boy, the registers
// are 8-bits in length, however,
// they can be combined to form
// a single 16-bit register
// to store a 16-bit value if needed.
//
// https://gbdev.io/pandocs/CPU_Registers_and_Flags.html

// Game Boy Register Map
// 16-bit	Hi	Lo	Name/Function
// AF	    A	  -	  Accumulator & Flags
// BC	    B	  C	  BC
// DE	    D	  E	  DE
// HL	    H	  L	  HL
// SP	    -	  -	  Stack Pointer
// PC	    -	  -	  Program Counter/Pointer

// F Register Map
// Bit	Name	Explanation
// 7	  z	    Zero flag
// 6	  n	    Subtraction flag (BCD)
// 5	  h	    Half Carry flag (BCD)
// 4	  c	    Carry flag

// Since the Game Boy can either
// be accessed as a single 8-bit register,
// or as a pair of two 8-bit registers
// treated as a single 16-bit register,
// we will refer to the 8-bit register
// as a ByteRegister, and a 16-bit register
// as a WordRegister

static constexpr uint8_t CPU_FLAG_MASK        = 0xF0;
static constexpr uint8_t ZERO_FLAG_BIT        = 0x7;
static constexpr uint8_t SUBTRACTION_FLAG_BIT = 0x6;
static constexpr uint8_t HALF_CARRY_FLAG_BIT   = 0x5;
static constexpr uint8_t CARRY_FLAG_BIT       = 0x4;

class ByteRegister : NoCopy
{
    protected:
        uint8_t value = 0x0;
    public:
        ByteRegister() = default;
        virtual ~ByteRegister() = default;

        virtual void set(const uint8_t newValue);
        void reset();
        uint8_t getValue() const;

        bool checkBit(const uint8_t bit) const;
        //uint8_t setBit(const uint8_t bit, const bool set);
        //uint8_t clearBit(const uint8_t bit);
        void setBitTo(const uint8_t bit, const bool set);

        void increment();
        void decrement();

        bool operator == (uint8_t other);
};

class FlagRegister : public ByteRegister
{
    public:
        FlagRegister() = default;

        // Specify operations for the flag register.
        void set(const uint8_t newValue) override;

        void setZeroFlag(const bool set);
        void setSubtractionFlag(const bool set);
        void setHalfCarryFlag(const bool set);
        void setCarryFlag(const bool set);

        bool zeroFlag() const;
        bool subtractionFlag() const;
        bool halfCarryFlag() const;
        bool carryFlag() const;

        uint8_t getZeroFlagValue() const;
        uint8_t getSubtractionFlagValue() const;
        uint8_t getHalfCarryFlagValue() const;
        uint8_t getCarryFlagValue() const;
};

class WordValue : NoCopy
{
    public:
        WordValue() = default;
        virtual ~WordValue() = default;

        virtual void set(uint16_t newValue) = 0;
        virtual uint16_t getValue() const = 0;
        virtual uint8_t getLowByte() const = 0;
        virtual uint8_t getHighByte() const = 0;
};

class WordRegister : public WordValue
{
    protected:
        uint16_t value = 0;
    public:
        WordRegister() = default;

        void set(const uint16_t newValue) override;
        uint16_t getValue() const override;
        uint8_t getLowByte() const override;
        uint8_t getHighByte() const override;

        void increment();
        void decrement();
};

class RegisterPair : public WordValue
{
    protected:
        ByteRegister &highByte;
        ByteRegister &lowByte;
    public:
        RegisterPair(ByteRegister &highByte, ByteRegister &lowByte);
        void set(const uint16_t word) override;
        uint8_t getLowByte() const override;
        uint8_t getHighByte() const override;
        uint16_t getValue() const override;

        void increment();
        void decrement();
};
