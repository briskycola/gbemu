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
    // TODO
    return;
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
}

// EI
void CPU::opcode_ei()
{
    isInterruptEnabled = true;
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
    isHalted = true;
}

// LD
void CPU::opcode_ld(ByteRegister &reg)
{
    // TODO
    return;
}

void CPU::opcode_ld(ByteRegister &reg, const ByteRegister &byteReg)
{
    // TODO
    return;
}

void CPU::opcode_ld(ByteRegister &reg, const Address &address)
{
    // TODO
    return;
}

void opcode_ld(RegisterPair& reg)
{
    // TODO
    return;
}
void opcode_ld(WordRegister& reg)
{
    // TODO
    return;
}

void opcode_ld(WordRegister &reg, const RegisterPair &regPair)
{
    // TODO
    return;
}

void opcode_ld(const Address &address)
{
    // TODO
    return;
}

void opcode_ld(const Address &address, const ByteRegister &byteReg)
{
    // TODO
    return;
}

void opcode_ld(const Address &address, const WordRegister &wordReg)
{
    // TODO
    return;
}

// (nn), A
void opcode_ld_to_addr(const ByteRegister &reg)
{
    // TODO
    return;
}
void opcode_ld_from_addr(ByteRegister &reg)
{
    // TODO
    return;
}

// LDD
uint8_t _opcode_ldd(uint8_t value)
{
    // TODO
    return 0x0;
}

void opcode_ldd(ByteRegister &reg, const Address &address)
{
    // TODO
    return;
}

void opcode_ldd(const Address &address, const ByteRegister &reg)
{
    // TODO
    return;
}

// LDH

// A, (n)
void opcode_ldh_into_a()
{
    // TODO
    return;
}

// (n), A
void opcode_ldh_into_data()
{
    // TODO
    return;
}

// (reg), A
void opcode_ldh_into_c()
{
    // TODO
    return;
}

// A, (reg)
void opcode_ldh_c_into_a()
{
    // TODO
    return;
}

// LDHL
void opcode_ldhl()
{
    // TODO
    return;
}

// LDI
void opcode_ldi(ByteRegister &reg, const Address &address)
{
    // TODO
    return;
}

void opcode_ldi(const Address &address, const ByteRegister &reg)
{
    // TODO
    return;
}

// NOP
void CPU::opcode_nop()
{
    // Do nothing
    return;
}

// OR
void _opcode_or(uint8_t value)
{
    // TODO
    return;
}

void opcode_or()
{
    // TODO
    return;
}

void opcode_or(const ByteRegister &reg)
{
    // TODO
    return;
}

void opcode_or(const Address &address)
{
    // TODO
    return;
}

// POP
void opcode_pop(RegisterPair &reg)
{
    // TODO
    return;
}

// PUSH
void opcode_push(const RegisterPair &reg)
{
    // TODO
    return;
}

// RES
void opcode_res(uint8_t bit, ByteRegister &reg)
{
    // TODO
    return;
}

void opcode_res(uint8_t bit, Address &&address)
{
    // TODO
    return;
}

// RET
void opcode_ret()
{
    // TODO
    return;
}

void opcode_ret(Condition condition)
{
    // TODO
    return;
}

// RETI
void opcode_reti()
{
    // TODO
    return;
}

// RL
uint8_t _opcode_rl(uint8_t value)
{
    // TODO
    return 0x0;
}

void opcode_rla()
{
    // TODO
    return;
}

void opcode_rl(ByteRegister &reg)
{
    // TODO
    return;
}

void opcode_rl(Address &&address)
{
    // TODO
    return;
}

// RLC
uint8_t _opcode_rlc(uint8_t value)
{
    // TODO
    return 0x0;
}

void opcode_rlca()
{
    // TODO
    return;
}

void opcode_rlc(ByteRegister &reg)
{
    // TODO
    return;
}

void opcode_rlc(Address &&address)
{
    // TODO
    return;
}

// RR
uint8_t _opcode_rr(uint8_t value)
{
    // TODO
    return 0x0;
}

void opcode_rra()
{
    // TODO
    return;
}

void opcode_rr(ByteRegister &reg)
{
    // TODO
    return;
}

void opcode_rr(Address &&address)
{
    // TODO
    return;
}

// RRC
uint8_t _opcode_rrc(uint8_t value)
{
    // TODO
    return 0x0;
}

void opcode_rrca()
{
    // TODO
    return;
}

void opcode_rrc(ByteRegister &reg)
{
    // TODO
    return;
}

void opcode_rrc(Address &&address)
{
    // TODO
    return;
}

// RST
void opcode_rst(uint8_t offset)
{
    // TODO
    return;
}

// SCF
void opcode_scf()
{
    // TODO
    return;
}

// SET
void opcode_set(uint8_t bit, ByteRegister &reg)
{
    // TODO
    return;
}

void opcode_set(uint8_t bit, Address &&address)
{
    // TODO
    return;
}

// SLA
uint8_t _opcode_sla(uint8_t value)
{
    // TODO
    return 0x0;
}

void opcode_sla(ByteRegister &reg)
{
    // TODO
    return;
}

void opcode_sla(Address &&address)
{
    // TODO
    return;
}

// SRA
uint8_t _opcode_sra(uint8_t value)
{
    // TODO
    return 0x0;
}

void opcode_sra(ByteRegister &reg)
{
    // TODO
    return;
}

void opcode_sra(Address &&address)
{
    // TODO
    return;
}

// SRL
uint8_t _opcode_srl(uint8_t value)
{
    // TODO
    return 0x0;
}

void opcode_srl(ByteRegister &reg)
{
    // TODO
    return;
}

void opcode_srl(Address &&address)
{
    // TODO
    return;
}

// STOP
void opcode_stop()
{
    // TODO
    return;
}

// SUB
void _opcode_sub(uint8_t value)
{
    // TODO
    return;
}

// SWAP
uint8_t _opcode_swap(uint8_t value)
{
    // TODO
    return 0x0;
}

void opcode_swap(ByteRegister &reg)
{
    // TODO
    return;
}

void opcode_swap(Address &&address)
{
    // TODO
    return;
}

// XOR
void _opcode_xor(uint8_t value)
{
    // TODO
    return;
}

void opcode_xor()
{
    // TODO
    return;
}

void opcode_xor(const ByteRegister &reg)
{
    // TODO
    return;
}

void opcode_xor(const Address &address)
{
    // TODO
    return;
}
