#include "common.hpp"
#include "cpu.hpp"
#include "emulator.hpp"
#include "register.hpp"

// ADC
void CPU::_opcode_adc(uint8_t value)
{
    uint8_t reg = a.getValue();
    uint8_t carry = f.getCarryFlagValue();

    uint32_t fullResult = reg + value + carry;
    uint8_t result = static_cast<uint8_t>(fullResult);

    setFlagZero(result == 0);
    setFlagSubtract(false);
    setFlagHalfCarry(((reg & 0xF) + (value & 0xF) + carry) > 0xF);
    setFlagCarry(fullResult > 0xFF);

    a.set(result);
}

void CPU::opcode_adc()
{
    _opcode_adc(getByteFromPC());
}

void CPU::opcode_adc(const ByteRegister &reg)
{
    _opcode_adc(reg.getValue());
}

void CPU::opcode_adc(const Address &&address)
{
    _opcode_adc(emulator.mmu.read(address));
}

// ADD
void CPU::_opcode_add(uint8_t reg, uint8_t value)
{
    uint32_t result = reg + value;
    a.set(static_cast<uint8_t>(result));

    setFlagZero(a.getValue() == 0);
    setFlagSubtract(false);
    setFlagHalfCarry((reg & 0xF) + (value & 0xF) > 0xF);
    setFlagCarry((result & 0x100) != 0);
}

void CPU::opcode_add_a()
{
    _opcode_add(a.getValue(), getByteFromPC());
}

void CPU::opcode_add_a(const ByteRegister &reg)
{
    _opcode_add(a.getValue(), reg.getValue());
}

void CPU::opcode_add_a(const Address &address)
{
    _opcode_add(a.getValue(), emulator.mmu.read(address));
}

void CPU::_opcode_add_hl(uint16_t value)
{
    uint16_t reg = hl.getValue();
    uint32_t result = reg + value;

    setFlagSubtract(false);
    setFlagHalfCarry((reg & 0xFFF) + (value & 0xFFF) > 0xFFF);
    setFlagCarry((result & 0x10000) != 0);

    hl.set(static_cast<uint16_t>(result));
}

void CPU::opcode_add_hl(const RegisterPair &regPair)
{
    _opcode_add_hl(regPair.getValue());
}

void CPU::opcode_add_hl(const WordRegister &wordReg)
{
    _opcode_add_hl(wordReg.getValue());
}

void CPU::opcode_add_sp()
{
    uint16_t reg = sp.getValue();
    int8_t value = getSignedByteFromPC();

    int32_t result = static_cast<int32_t>(reg + value);

    setFlagZero(false);
    setFlagSubtract(false);
    setFlagHalfCarry(((reg ^ value ^ (result & 0xFFFF)) & 0x10) == 0x10);
    setFlagCarry(((reg ^ value ^ (result & 0xFFFF)) & 0x100) == 0x100);

    sp.set(static_cast<uint16_t>(result));
}

// AND
void CPU::_opcode_and(uint8_t value)
{
    uint8_t reg = a.getValue();
    uint8_t result = reg & value;

    a.set(result);

    setFlagZero(a.getValue() == 0);
    setFlagSubtract(false);
    setFlagHalfCarry(true);
    setFlagCarry(false);
}

void CPU::opcode_and()
{
    _opcode_and(getByteFromPC());
}

void CPU::opcode_and(ByteRegister &reg)
{
    _opcode_and(reg.getValue());
}

void CPU::opcode_and(Address &&addr)
{
    _opcode_and(emulator.mmu.read(addr));
}

// BIT
void CPU::_opcode_bit(const uint8_t bit, const uint8_t value)
{
    setFlagZero(!checkBit(value, bit));
    setFlagSubtract(false);
    setFlagHalfCarry(true);
}

void CPU::opcode_bit(const uint8_t bit, ByteRegister &reg)
{
    _opcode_bit(bit, reg.getValue());
}

void CPU::opcode_bit(const uint8_t bit, Address &&address)
{
    _opcode_bit(bit, emulator.mmu.read(address));
}

// CALL
void CPU::opcode_call()
{
    uint16_t address = getWordFromPC();
    stackPush(pc);
    pc.set(address);
}

void CPU::opcode_call(Condition condition)
{
    if (isConditon(condition))
    {
        opcode_call();
    }

    else
    {
        getWordFromPC();
    }
}

// CCF
void CPU::opcode_ccf()
{
    setFlagSubtract(false);
    setFlagHalfCarry(false);
    setFlagCarry(!f.CarryFlag());
}

// CP
void CPU::_opcode_cp(const uint8_t value)
{
    uint8_t reg = a.getValue();
    uint8_t result = static_cast<uint8_t>(reg - value);

    setFlagZero(result == 0);
    setFlagSubtract(true);
    setFlagHalfCarry(((reg & 0xF) - (value & 0xF)) < 0);
    setFlagCarry(reg < value);
}

void CPU::opcode_cp()
{
    _opcode_cp(getByteFromPC());
}

void CPU::opcode_cp(const ByteRegister &reg)
{
    _opcode_cp(reg.getValue());
}

void CPU::opcode_cp(const Address &address)
{
    _opcode_cp(emulator.mmu.read(address));
}

// CPL
void CPU::opcode_cpl()
{
    uint8_t reg = a.getValue();
    uint8_t result = ~reg;
    a.set(result);

    setFlagSubtract(true);
    setFlagHalfCarry(true);
}

// DAA
void CPU::opcode_daa()
{
    uint8_t reg = a.getValue();

    uint16_t correction = f.CarryFlag() ? 0x60 : 0x00;

    if (f.HalfCarryFlag() || !f.SubtractionFlag() && ((reg & 0x0F) > 9))
    {
        correction |= 0x06;
    }

    if (f.CarryFlag() || (!f.SubtractionFlag() && (reg > 0x99)))
    {
        correction |= 0x60;
    }

    if (f.SubtractionFlag())
    {
        reg = static_cast<uint8_t>(reg - correction);
    }

    else
    {
        reg = static_cast<uint8_t>(reg + correction);
    }

    if (((correction << 2) & 0x100) != 0)
    {
        setFlagCarry(true);
    }

    setFlagZero(reg == 0);
    setFlagHalfCarry(false);
    a.set(static_cast<uint8_t>(reg));
}

// DEC
void CPU::opcode_dec(ByteRegister &reg)
{
    reg.decrement();

    setFlagZero(reg.getValue() == 0);
    setFlagSubtract(true);
    setFlagHalfCarry((reg.getValue() & 0x0F) == 0x0F);
}

void CPU::opcode_dec(RegisterPair &reg)
{
    reg.decrement();
}

void CPU::opcode_dec(WordRegister &reg)
{
    reg.decrement();
}

void CPU::opcode_dec(Address &&addr)
{
    uint8_t value = emulator.mmu.read(addr);
    uint8_t result = static_cast<uint8_t>(value - 1);
    emulator.mmu.write(addr, result);

    setFlagZero(result == 0);
    setFlagSubtract(true);
    setFlagHalfCarry((result & 0x0F) == 0x0F);
}

// DI
void CPU::opcode_di()
{
    isInterruptEnabled = false;
    imeEnableDelay = 0;
}

// EI
void CPU::opcode_ei()
{
    imeEnableDelay = 2;
}

// INC
void CPU::opcode_inc(ByteRegister &reg)
{
    reg.increment();

    setFlagZero(reg.getValue() == 0);
    setFlagSubtract(false);
    setFlagHalfCarry((reg.getValue() & 0x0F) == 0x00);
}

void CPU::opcode_inc(RegisterPair &reg)
{
    reg.increment();
}

void CPU::opcode_inc(WordRegister &reg)
{
    reg.increment();
}

void CPU::opcode_inc(Address &&address)
{
    uint8_t value = emulator.mmu.read(address);
    uint8_t result = static_cast<uint8_t>(value + 1);
    emulator.mmu.write(address, result);

    setFlagZero(result == 0);
    setFlagSubtract(false);
    setFlagHalfCarry((result & 0x0F) == 0x00);
}

// JP
void CPU::opcode_jp()
{
    uint16_t address = getWordFromPC();
    pc.set(address);
}

void CPU::opcode_jp(Condition condition)
{
    if (isConditon(condition))
    {
        opcode_jp();
    }

    else
    {
        getWordFromPC();
    }
}

void CPU::opcode_jp(const Address &address)
{
    pc.set(hl.getValue());
}

// JR
void CPU::opcode_jr()
{
    int8_t offset = getSignedByteFromPC();

    uint16_t old_pc = pc.getValue();
    uint16_t new_pc = static_cast<uint16_t>(old_pc + offset);
    pc.set(new_pc);
}

void CPU::opcode_jr(Condition condition)
{
    if (isConditon(condition))
    {
        opcode_jr();
    }

    else
    {
        getSignedByteFromPC();
    }
}

// HALT
void CPU::opcode_halt()
{
    const uint8_t pendingInterrupts =
        emulator.mmu.read(Address(0xFF0F)) & emulator.mmu.read(Address(0xFFFF)) & 0x1F;

    if (!isInterruptEnabled && pendingInterrupts != 0)
    {
        if (imeEnableDelay > 0)
        {
            isInterruptEnabled = true;
            imeEnableDelay = 0;
        }
        else
        {
            haltBug = true;
        }
    }
    else
    {
        isHalted = true;
    }
}

// LD
void CPU::opcode_ld(ByteRegister &reg)
{
    uint8_t byte = getByteFromPC();
    reg.set(byte);
}

void CPU::opcode_ld(ByteRegister &reg, const ByteRegister &byteReg)
{
    reg.set(byteReg.getValue());
}

void CPU::opcode_ld(ByteRegister &reg, const Address &address)
{
    reg.set(emulator.mmu.read(address));
}

void CPU::opcode_ld(RegisterPair& reg)
{
    uint16_t word = getWordFromPC();
    reg.set(word);
}

void CPU::opcode_ld(WordRegister& reg)
{
    uint16_t word = getWordFromPC();
    reg.set(word);
}

void CPU::opcode_ld(WordRegister &reg, const RegisterPair &regPair)
{
    reg.set(regPair.getValue());
}

void CPU::opcode_ld(const Address &address)
{
    uint8_t byte = getByteFromPC();
    emulator.mmu.write(address, byte);
}

void CPU::opcode_ld(const Address &address, const ByteRegister &byteReg)
{
    emulator.mmu.write(address, byteReg.getValue());
}

void CPU::opcode_ld(const Address &address, const WordRegister &wordReg)
{
    emulator.mmu.write(address, wordReg.getLowByte());
    emulator.mmu.write(address + 1, wordReg.getHighByte());
}

// (nn), A
void CPU::opcode_ld_to_addr(const ByteRegister &reg)
{
    uint16_t address = getWordFromPC();
    emulator.mmu.write(Address(address), reg.getValue());
}

void CPU::opcode_ld_from_addr(ByteRegister &reg)
{
    uint16_t word = getWordFromPC();
    reg.set(emulator.mmu.read(word));
}

// LDD
//uint8_t CPU::_opcode_ldd(uint8_t value)
//{
//    reg.set(emu)
//}

void CPU::opcode_ldd(ByteRegister &reg, const Address &address)
{
    reg.set(emulator.mmu.read(address));
    hl.decrement();
}

void CPU::opcode_ldd(const Address &address, const ByteRegister &reg)
{
    emulator.mmu.write(address, reg.getValue());
    hl.decrement();
}

// LDH

// A, (n)
void CPU::opcode_ldh_into_a()
{
    uint8_t offset = getByteFromPC();
    Address address = Address(0xFF00 + offset);

    uint8_t value = emulator.mmu.read(address);
    a.set(value);
}

// (n), A
void CPU::opcode_ldh_into_data()
{
    uint8_t offset = getByteFromPC();
    Address address = Address(0xFF00 + offset);

    emulator.mmu.write(address, a.getValue());
}

// (reg), A
void CPU::opcode_ldh_into_c()
{
    uint8_t offset = c.getValue();
    Address address = Address(0xFF00 + offset);

    emulator.mmu.write(address, a.getValue());
}

// A, (reg)
void CPU::opcode_ldh_c_into_a()
{
    Address address = Address(0xFF00 + c.getValue());
    a.set(emulator.mmu.read(address));
}

// LDHL
void CPU::opcode_ldhl()
{
    uint16_t reg = sp.getValue();
    int8_t value = getSignedByteFromPC();
    int32_t result = static_cast<int32_t>(reg + value);

    setFlagZero(false);
    setFlagSubtract(false);
    setFlagHalfCarry(((reg ^ value ^ (result & 0xFFFF)) & 0x10) == 0x10);
    setFlagCarry(((reg ^ value ^ (result & 0xFFFF)) & 0x100) == 0x100);

    hl.set(static_cast<uint16_t>(result));
}

// LDI
void CPU::opcode_ldi(ByteRegister &reg, const Address &address)
{
    reg.set(emulator.mmu.read(address));
    hl.increment();
}

void CPU::opcode_ldi(const Address &address, const ByteRegister &reg)
{
    emulator.mmu.write(address, reg.getValue());
    hl.increment();
}

// NOP
void CPU::opcode_nop()
{
    // Do nothing
    return;
}

// OR
void CPU::_opcode_or(uint8_t value)
{
    uint8_t reg = a.getValue();
    uint8_t result = reg | value;

    a.set(result);

    setFlagZero(a.getValue() == 0);
    setFlagSubtract(false);
    setFlagHalfCarry(false);
    setFlagCarry(false);
}

void CPU::opcode_or()
{
    _opcode_or(getByteFromPC());
}

void CPU::opcode_or(const ByteRegister &reg)
{
    _opcode_or(reg.getValue());
}

void CPU::opcode_or(const Address &address)
{
    _opcode_or(emulator.mmu.read(address));
}

// POP
void CPU::opcode_pop(RegisterPair &reg)
{
    stackPop(reg);
}

// PUSH
void CPU::opcode_push(const RegisterPair &reg)
{
    stackPush(reg);
}

// RES
void CPU::opcode_res(uint8_t bit, ByteRegister &reg)
{
    uint8_t result = clearBit(reg.getValue(), bit);
    reg.set(result);
}

void CPU::opcode_res(uint8_t bit, Address &&address)
{
    uint8_t value = emulator.mmu.read(address);
    uint8_t result = clearBit(value, bit);
    emulator.mmu.write(address, result);
}

// RET
void CPU::opcode_ret()
{
    stackPop(pc);
}

void CPU::opcode_ret(Condition condition)
{
    if (isConditon(condition))
    {
        opcode_ret();
    }
}

// RETI
void CPU::opcode_reti()
{
    opcode_ret();
    isInterruptEnabled = true;
    imeEnableDelay = 0;
}

// RL
uint8_t CPU::_opcode_rl(uint8_t value)
{
    uint8_t carry = f.getCarryFlagValue();
    bool willCarry = checkBit(value, 7);
    uint8_t result = static_cast<uint8_t>(value << 1);

    setFlagCarry(willCarry);
    result |= carry;

    setFlagZero(result == 0);
    setFlagSubtract(false);
    setFlagHalfCarry(false);

    return result;
}

void CPU::opcode_rl(ByteRegister &reg)
{
    uint8_t result = _opcode_rl(reg.getValue());
    reg.set(result);
}

void CPU::opcode_rl(Address &&address)
{
    uint8_t result = _opcode_rl(emulator.mmu.read(address));
    emulator.mmu.write(address, result);
}

// RLA
void CPU::opcode_rla()
{
    opcode_rl(a);
    setFlagZero(false);
}

// RLC
uint8_t CPU::_opcode_rlc(uint8_t value)
{
    uint8_t carryFlag = checkBit(value, 7);
    uint8_t truncatedBit = checkBit(value, 7);
    uint8_t result = static_cast<uint8_t>((value << 1) | truncatedBit);

    setFlagZero(result == 0);
    setFlagSubtract(false);
    setFlagHalfCarry(false);
    setFlagCarry(carryFlag);

    return result;
}

void CPU::opcode_rlc(ByteRegister &reg)
{
    uint8_t result = _opcode_rlc(reg.getValue());
    reg.set(result);
}

void CPU::opcode_rlc(Address &&address)
{
    uint8_t result = _opcode_rlc(emulator.mmu.read(address));
    emulator.mmu.write(address, result);
}

// RLCA
void CPU::opcode_rlca()
{
    opcode_rlc(a);
    setFlagZero(false);
}

// RR
uint8_t CPU::_opcode_rr(uint8_t value)
{
    uint8_t carry = f.getCarryFlagValue();
    bool willCarry = checkBit(value, 0);
    uint8_t result = static_cast<uint8_t>(value >> 1);

    result |= (carry << 7);

    setFlagZero(result == 0);
    setFlagSubtract(false);
    setFlagHalfCarry(false);
    setFlagCarry(willCarry);

    return result;
}

void CPU::opcode_rr(ByteRegister &reg)
{
    uint8_t result = _opcode_rr(reg.getValue());
    reg.set(result);
}

void CPU::opcode_rr(Address &&address)
{
    uint8_t result = _opcode_rr(emulator.mmu.read(address));
    emulator.mmu.write(address, result);
}

// RRA
void CPU::opcode_rra()
{
    opcode_rr(a);
    setFlagZero(false);
}

// RRC
uint8_t CPU::_opcode_rrc(uint8_t value)
{
    uint8_t carryFlag = checkBit(value, 0);
    uint8_t truncatedBit = checkBit(value, 0);
    uint8_t result = static_cast<uint8_t>((value >> 1) | (truncatedBit << 7));

    setFlagZero(result == 0);
    setFlagSubtract(false);
    setFlagHalfCarry(false);
    setFlagCarry(carryFlag);

    return result;
}

void CPU::opcode_rrc(ByteRegister &reg)
{
    uint8_t result = _opcode_rrc(reg.getValue());
    reg.set(result);
}

void CPU::opcode_rrc(Address &&address)
{
    uint8_t result = _opcode_rrc(emulator.mmu.read(address));
    emulator.mmu.write(address, result);
}

// RRCA
void CPU::opcode_rrca()
{
    opcode_rrc(a);
    setFlagZero(false);
}

// RST
void CPU::opcode_rst(uint8_t offset)
{
    stackPush(pc);
    pc.set(offset);
}

// SBC
void CPU::_opcode_sbc(const uint8_t value)
{
    uint8_t carry = f.getCarryFlagValue();
    uint8_t reg = a.getValue();

    int32_t fullResult = reg - value - carry;
    uint8_t result = static_cast<uint8_t>(fullResult);

    setFlagZero(result == 0);
    setFlagSubtract(true);
    setFlagHalfCarry(((reg & 0xF) - (value & 0xF) - carry) < 0);
    setFlagCarry(fullResult < 0);

    a.set(result);
}

void CPU::opcode_sbc()
{
    _opcode_sbc(getByteFromPC());
}

void CPU::opcode_sbc(ByteRegister &reg)
{
    _opcode_sbc(reg.getValue());
}

void CPU::opcode_sbc(Address &&address)
{
    _opcode_sbc(emulator.mmu.read(address));
}

// SCF
void CPU::opcode_scf()
{
    setFlagSubtract(false);
    setFlagHalfCarry(false);
    setFlagCarry(true);
}

// SET
void CPU::opcode_set(uint8_t bit, ByteRegister &reg)
{
    uint8_t result = setBit(reg.getValue(), bit);
    reg.set(result);
}

void CPU::opcode_set(uint8_t bit, Address &&address)
{
    uint8_t value = emulator.mmu.read(address);
    uint8_t result = setBit(value, bit);
    emulator.mmu.write(address, result);
}

// SLA
uint8_t CPU::_opcode_sla(uint8_t value)
{
    uint8_t carryBit = checkBit(value, 7);
    uint8_t result = static_cast<uint8_t>(value << 1);

    setFlagZero(result == 0);
    setFlagSubtract(false);
    setFlagHalfCarry(false);
    setFlagCarry(carryBit);

    return result;
}

void CPU::opcode_sla(ByteRegister &reg)
{
    uint8_t result = _opcode_sla(reg.getValue());
    reg.set(result);
}

void CPU::opcode_sla(Address &&address)
{
    uint8_t result = _opcode_sla(emulator.mmu.read(address));
    emulator.mmu.write(address, result);
}

// SRA
uint8_t CPU::_opcode_sra(uint8_t value)
{
    uint8_t carryBit = checkBit(value, 0);
    uint8_t topBit = checkBit(value, 7);
    uint8_t result = static_cast<uint8_t>(value >> 1);

    result = setBitTo(result, 7, topBit);

    setFlagZero(result == 0);
    setFlagSubtract(false);
    setFlagHalfCarry(false);
    setFlagCarry(carryBit);

    return result;
}

void CPU::opcode_sra(ByteRegister &reg)
{
    uint8_t result = _opcode_sra(reg.getValue());
    reg.set(result);
}

void CPU::opcode_sra(Address &&address)
{
    uint8_t result = _opcode_sra(emulator.mmu.read(address));
    emulator.mmu.write(address, result);
}

// SRL
uint8_t CPU::_opcode_srl(uint8_t value)
{
    bool leastBitSet = checkBit(value, 0);
    uint8_t result = (value >> 1);

    setFlagZero(result == 0);
    setFlagSubtract(false);
    setFlagHalfCarry(false);
    setFlagCarry(leastBitSet);

    return result;
}

void CPU::opcode_srl(ByteRegister &reg)
{
    uint8_t result = _opcode_srl(reg.getValue());
    reg.set(result);
}

void CPU::opcode_srl(Address &&address)
{
    uint8_t result = _opcode_srl(emulator.mmu.read(address));
    emulator.mmu.write(address, result);
}

// STOP
void CPU::opcode_stop()
{
    isHalted = true;
}

// SUB
void CPU::_opcode_sub(uint8_t value)
{
    uint8_t reg = a.getValue();
    uint8_t result = static_cast<uint8_t>(reg - value);

    a.set(result);

    setFlagZero(a.getValue() == 0);
    setFlagSubtract(true);
    setFlagHalfCarry(((reg & 0xF) - (value & 0xF)) < 0);
    setFlagCarry(reg < value);
}

void CPU::opcode_sub()
{
    _opcode_sub(getByteFromPC());
}

void CPU::opcode_sub(ByteRegister &reg)
{
    _opcode_sub(reg.getValue());
}

void CPU::opcode_sub(Address &&address)
{
    _opcode_sub(emulator.mmu.read(address));
}

// SWAP
uint8_t CPU::_opcode_swap(uint8_t value)
{
    uint8_t lowerNibble = value & 0x0F;
    uint8_t upperNibble = (value & 0xF0) >> 4;
    uint8_t result = composeNibbles(lowerNibble, upperNibble);

    setFlagZero(result == 0);
    setFlagSubtract(false);
    setFlagHalfCarry(false);
    setFlagCarry(false);

    return result;
}

void CPU::opcode_swap(ByteRegister &reg)
{
    uint8_t result = _opcode_swap(reg.getValue());
    reg.set(result);
}

void CPU::opcode_swap(Address &&address)
{
    uint8_t result = _opcode_swap(emulator.mmu.read(address));
    emulator.mmu.write(address, result);
}

// XOR
void CPU::_opcode_xor(uint8_t value)
{
    uint8_t reg = a.getValue();
    uint8_t result = reg ^ value;

    setFlagZero(result == 0);
    setFlagSubtract(false);
    setFlagHalfCarry(false);
    setFlagCarry(false);

    a.set(result);
}

void CPU::opcode_xor()
{
    _opcode_xor(getByteFromPC());
}

void CPU::opcode_xor(const ByteRegister &reg)
{
    _opcode_xor(reg.getValue());
}

void CPU::opcode_xor(const Address &address)
{
    _opcode_xor(emulator.mmu.read(address));
}
