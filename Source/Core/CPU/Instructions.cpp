#include "Core/Common/Common.hpp"
#include "Core/CPU/CPU.hpp"
#include "Core/Emulator.hpp"
#include "Core/Common/Register.hpp"
#include "Core/Memory/IORegisters.hpp"
#include "Core/Memory/MMU.hpp"

// The CPU instructions. Results and flags follow Pan Docs:
// https://gbdev.io/pandocs/CPU_Instruction_Set.html
// https://gbdev.io/pandocs/CPU_Registers_and_Flags.html
//
// Every memory access goes through busRead/busWrite, which take one machine
// cycle each, so the order of the accesses here is their real order on the bus.

// ADC
void CPU::opcodeAdcImpl(uint8_t value)
{
    uint8_t reg = a.getValue();
    uint8_t carry = f.getCarryFlagValue();

    uint32_t fullResult = reg + value + carry;
    uint8_t result = static_cast<uint8_t>(fullResult);

    setFlagZero(result == 0);
    setFlagSubtract(false);
    // Half carry: the low nibbles overflow past bit 3.
    // Carry: the sum overflows past bit 7.
    setFlagHalfCarry(((reg & 0xF) + (value & 0xF) + carry) > 0xF);
    setFlagCarry(fullResult > 0xFF);

    a.set(result);
}

void CPU::opcodeAdc()
{
    opcodeAdcImpl(getByteFromPC());
}

void CPU::opcodeAdc(const ByteRegister &reg)
{
    opcodeAdcImpl(reg.getValue());
}

void CPU::opcodeAdc(const Address &&address)
{
    opcodeAdcImpl(busRead(address));
}

// ADD
void CPU::opcodeAddImpl(uint8_t reg, uint8_t value)
{
    uint32_t result = reg + value;
    a.set(static_cast<uint8_t>(result));

    setFlagZero(a.getValue() == 0);
    setFlagSubtract(false);
    // Half carry out of bit 3, carry out of bit 7
    setFlagHalfCarry((reg & 0xF) + (value & 0xF) > 0xF);
    setFlagCarry((result & 0x100) != 0);
}

void CPU::opcodeAddA()
{
    opcodeAddImpl(a.getValue(), getByteFromPC());
}

void CPU::opcodeAddA(const ByteRegister &reg)
{
    opcodeAddImpl(a.getValue(), reg.getValue());
}

void CPU::opcodeAddA(const Address &address)
{
    opcodeAddImpl(a.getValue(), busRead(address));
}

void CPU::opcodeAddHLImpl(uint16_t value)
{
    uint16_t reg = hl.getValue();
    uint32_t result = reg + value;

    setFlagSubtract(false);
    // 16-bit add: half carry out of bit 11, carry out of bit 15
    setFlagHalfCarry((reg & 0xFFF) + (value & 0xFFF) > 0xFFF);
    setFlagCarry((result & 0x10000) != 0);

    hl.set(static_cast<uint16_t>(result));
}

void CPU::opcodeAddHL(const RegisterPair &regPair)
{
    opcodeAddHLImpl(regPair.getValue());
}

void CPU::opcodeAddHL(const WordRegister &wordReg)
{
    opcodeAddHLImpl(wordReg.getValue());
}

void CPU::opcodeAddSP()
{
    uint16_t reg = sp.getValue();
    int8_t value = getSignedByteFromPC();

    int32_t result = static_cast<int32_t>(reg + value);

    setFlagZero(false);
    setFlagSubtract(false);
    // SP + signed offset sets the flags from the low byte:
    // half carry out of bit 3 and carry out of bit 7
    setFlagHalfCarry(((reg ^ value ^ (result & 0xFFFF)) & 0x10) == 0x10);
    setFlagCarry(((reg ^ value ^ (result & 0xFFFF)) & 0x100) == 0x100);

    sp.set(static_cast<uint16_t>(result));
}

// AND
void CPU::opcodeAndImpl(uint8_t value)
{
    uint8_t reg = a.getValue();
    uint8_t result = reg & value;

    a.set(result);

    setFlagZero(a.getValue() == 0);
    setFlagSubtract(false);
    setFlagHalfCarry(true);
    setFlagCarry(false);
}

void CPU::opcodeAnd()
{
    opcodeAndImpl(getByteFromPC());
}

void CPU::opcodeAnd(ByteRegister &reg)
{
    opcodeAndImpl(reg.getValue());
}

void CPU::opcodeAnd(Address &&addr)
{
    opcodeAndImpl(busRead(addr));
}

// BIT
void CPU::opcodeBitImpl(const uint8_t bit, const uint8_t value)
{
    setFlagZero(!checkBit(value, bit));
    setFlagSubtract(false);
    setFlagHalfCarry(true);
}

void CPU::opcodeBit(const uint8_t bit, ByteRegister &reg)
{
    opcodeBitImpl(bit, reg.getValue());
}

void CPU::opcodeBit(const uint8_t bit, Address &&address)
{
    opcodeBitImpl(bit, busRead(address));
}

// CALL
void CPU::opcodeCall()
{
    uint16_t address = getWordFromPC();
    stackPush(pc);
    pc.set(address);
}

void CPU::opcodeCall(Condition condition)
{
    if (isConditon(condition))
    {
        opcodeCall();
    }

    else
    {
        getWordFromPC();
    }
}

// CCF
void CPU::opcodeCcf()
{
    setFlagSubtract(false);
    setFlagHalfCarry(false);
    setFlagCarry(!f.carryFlag());
}

// CP
void CPU::opcodeCpImpl(const uint8_t value)
{
    uint8_t reg = a.getValue();
    uint8_t result = static_cast<uint8_t>(reg - value);

    setFlagZero(result == 0);
    setFlagSubtract(true);
    // Half carry: the low nibble needs a borrow from bit 4
    setFlagHalfCarry(((reg & 0xF) - (value & 0xF)) < 0);
    setFlagCarry(reg < value);
}

void CPU::opcodeCp()
{
    opcodeCpImpl(getByteFromPC());
}

void CPU::opcodeCp(const ByteRegister &reg)
{
    opcodeCpImpl(reg.getValue());
}

void CPU::opcodeCp(const Address &address)
{
    opcodeCpImpl(busRead(address));
}

// CPL
void CPU::opcodeCpl()
{
    uint8_t reg = a.getValue();
    uint8_t result = ~reg;
    a.set(result);

    setFlagSubtract(true);
    setFlagHalfCarry(true);
}

// DAA
void CPU::opcodeDaa()
{
    uint8_t reg = a.getValue();

    // Adjust A back into binary-coded decimal after an add or
    // subtract. 0x06 fixes the low digit and 0x60 the high digit
    // when it went past 9 (or carried/borrowed).
    uint16_t correction = f.carryFlag() ? 0x60 : 0x00;

    if (f.halfCarryFlag() || !f.subtractionFlag() && ((reg & 0x0F) > 9))
    {
        correction |= 0x06;
    }

    if (f.carryFlag() || (!f.subtractionFlag() && (reg > 0x99)))
    {
        correction |= 0x60;
    }

    if (f.subtractionFlag())
    {
        reg = static_cast<uint8_t>(reg - correction);
    }

    else
    {
        reg = static_cast<uint8_t>(reg + correction);
    }

    // A high-digit correction (0x60) means the result carried
    if (((correction << 2) & 0x100) != 0)
    {
        setFlagCarry(true);
    }

    setFlagZero(reg == 0);
    setFlagHalfCarry(false);
    a.set(static_cast<uint8_t>(reg));
}

// DEC
void CPU::opcodeDec(ByteRegister &reg)
{
    reg.decrement();

    setFlagZero(reg.getValue() == 0);
    setFlagSubtract(true);
    // Decrementing borrows from bit 4 when the low nibble wraps to 0xF
    setFlagHalfCarry((reg.getValue() & 0x0F) == 0x0F);
}

void CPU::opcodeDec(RegisterPair &reg)
{
    iduCycle(reg.getValue());
    reg.decrement();
}

void CPU::opcodeDec(WordRegister &reg)
{
    iduCycle(reg.getValue());
    reg.decrement();
}

void CPU::opcodeDec(Address &&addr)
{
    uint8_t value = busRead(addr);
    uint8_t result = static_cast<uint8_t>(value - 1);
    busWrite(addr, result);

    setFlagZero(result == 0);
    setFlagSubtract(true);
    // Decrementing borrows from bit 4 when the low nibble wraps to 0xF
    setFlagHalfCarry((result & 0x0F) == 0x0F);
}

// DI clears IME immediately and cancels a pending EI
// https://gbdev.io/pandocs/Interrupts.html#ime-interrupt-master-enable-flag-write-only
void CPU::opcodeDi()
{
    isInterruptEnabled = false;
    imeEnableDelay = 0;
}

// EI
// IME is set only after the instruction following EI. Another EI in the
// meantime doesn't restart that delay.
// https://gbdev.io/pandocs/Interrupts.html#ime-interrupt-master-enable-flag-write-only
void CPU::opcodeEi()
{
    if (!isInterruptEnabled && imeEnableDelay == 0) imeEnableDelay = cpu::EI_ENABLE_DELAY;
}

// INC
void CPU::opcodeInc(ByteRegister &reg)
{
    reg.increment();

    setFlagZero(reg.getValue() == 0);
    setFlagSubtract(false);
    // Incrementing carries into bit 4 when the low nibble wraps to 0x0
    setFlagHalfCarry((reg.getValue() & 0x0F) == 0x00);
}

void CPU::opcodeInc(RegisterPair &reg)
{
    iduCycle(reg.getValue());
    reg.increment();
}

void CPU::opcodeInc(WordRegister &reg)
{
    iduCycle(reg.getValue());
    reg.increment();
}

void CPU::opcodeInc(Address &&address)
{
    uint8_t value = busRead(address);
    uint8_t result = static_cast<uint8_t>(value + 1);
    busWrite(address, result);

    setFlagZero(result == 0);
    setFlagSubtract(false);
    // Incrementing carries into bit 4 when the low nibble wraps to 0x0
    setFlagHalfCarry((result & 0x0F) == 0x00);
}

// JP
void CPU::opcodeJp()
{
    uint16_t address = getWordFromPC();
    pc.set(address);
}

void CPU::opcodeJp(Condition condition)
{
    if (isConditon(condition))
    {
        opcodeJp();
    }

    else
    {
        getWordFromPC();
    }
}

void CPU::opcodeJp(const Address &address)
{
    pc.set(hl.getValue());
}

// JR
void CPU::opcodeJr()
{
    int8_t offset = getSignedByteFromPC();

    uint16_t oldPC = pc.getValue();
    uint16_t newPC = static_cast<uint16_t>(oldPC + offset);
    pc.set(newPC);
}

void CPU::opcodeJr(Condition condition)
{
    if (isConditon(condition))
    {
        opcodeJr();
    }

    else
    {
        getSignedByteFromPC();
    }
}

// HALT
// See https://gbdev.io/pandocs/halt.html
void CPU::opcodeHalt()
{
    const uint8_t pendingInterrupts =
        emulator.mmu.read(Address(io::INTERRUPT_FLAG_REGISTER)) &
        emulator.mmu.read(Address(mmu::INTERRUPT_ENABLE_REGISTER)) & cpu::INTERRUPT_MASK;

    // IME set, or nothing pending: halt normally. The CPU wakes once
    // [IE] & [IF] != 0 and, if IME is set, the interrupt is handled first.
    // An EI right before this HALT still takes effect afterwards as usual.
    if (isInterruptEnabled || pendingInterrupts == 0)
    {
        isHalted = true;
        return;
    }

    // HALT bug: IME = 0 and an interrupt is pending. HALT ends immediately
    // and PC fails to be incremented.
    if (imeEnableDelay > 0)
    {
        // EI immediately before HALT: the interrupt is serviced, but it
        // returns to the HALT, which runs again and waits for another
        // interrupt. Pointing PC back at the HALT makes the dispatch on the
        // next tick push the HALT's own address. This wins over a following RST.
        isInterruptEnabled = true;
        imeEnableDelay = 0;
        pc.decrement();
    }
    else
    {
        // The byte after HALT is read twice. If it is an RST, its pushed
        // return address points at the RST itself.
        haltBug = true;
    }
}

// LD
void CPU::opcodeLd(ByteRegister &reg)
{
    uint8_t byte = getByteFromPC();
    reg.set(byte);
}

void CPU::opcodeLd(ByteRegister &reg, const ByteRegister &byteReg)
{
    reg.set(byteReg.getValue());
}

void CPU::opcodeLd(ByteRegister &reg, const Address &address)
{
    reg.set(busRead(address));
}

void CPU::opcodeLd(RegisterPair& reg)
{
    uint16_t word = getWordFromPC();
    reg.set(word);
}

void CPU::opcodeLd(WordRegister& reg)
{
    uint16_t word = getWordFromPC();
    reg.set(word);
}

void CPU::opcodeLd(WordRegister &reg, const RegisterPair &regPair)
{
    reg.set(regPair.getValue());
}

void CPU::opcodeLd(const Address &address)
{
    uint8_t byte = getByteFromPC();
    busWrite(address, byte);
}

void CPU::opcodeLd(const Address &address, const ByteRegister &byteReg)
{
    busWrite(address, byteReg.getValue());
}

void CPU::opcodeLd(const Address &address, const WordRegister &wordReg)
{
    busWrite(address, wordReg.getLowByte());
    busWrite(address + 1, wordReg.getHighByte());
}

// (nn), A
void CPU::opcodeLdToAddr(const ByteRegister &reg)
{
    uint16_t address = getWordFromPC();
    busWrite(Address(address), reg.getValue());
}

void CPU::opcodeLdFromAddr(ByteRegister &reg)
{
    uint16_t word = getWordFromPC();
    reg.set(busRead(word));
}

// LDD
//uint8_t CPU::opcodeLddImpl(uint8_t value)
//{
//    reg.set(emu)
//}

void CPU::opcodeLdd(ByteRegister &reg, const Address &address)
{
    reg.set(busRead(address, OAMCorruption::ReadIncrease));
    hl.decrement();
}

void CPU::opcodeLdd(const Address &address, const ByteRegister &reg)
{
    busWrite(address, reg.getValue());
    hl.decrement();
}

// LDH

// A, (n)
void CPU::opcodeLdhIntoA()
{
    uint8_t offset = getByteFromPC();
    Address address = Address(mmu::IO_ADDRESS_START + offset);

    uint8_t value = busRead(address);
    a.set(value);
}

// (n), A
void CPU::opcodeLdhIntoData()
{
    uint8_t offset = getByteFromPC();
    Address address = Address(mmu::IO_ADDRESS_START + offset);

    busWrite(address, a.getValue());
}

// (reg), A
void CPU::opcodeLdhIntoC()
{
    uint8_t offset = c.getValue();
    Address address = Address(mmu::IO_ADDRESS_START + offset);

    busWrite(address, a.getValue());
}

// A, (reg)
void CPU::opcodeLdhCIntoA()
{
    Address address = Address(mmu::IO_ADDRESS_START + c.getValue());
    a.set(busRead(address));
}

// LDHL
void CPU::opcodeLdhl()
{
    uint16_t reg = sp.getValue();
    int8_t value = getSignedByteFromPC();
    int32_t result = static_cast<int32_t>(reg + value);

    setFlagZero(false);
    setFlagSubtract(false);
    // SP + signed offset sets the flags from the low byte:
    // half carry out of bit 3 and carry out of bit 7
    setFlagHalfCarry(((reg ^ value ^ (result & 0xFFFF)) & 0x10) == 0x10);
    setFlagCarry(((reg ^ value ^ (result & 0xFFFF)) & 0x100) == 0x100);

    hl.set(static_cast<uint16_t>(result));
}

// LDI
void CPU::opcodeLdi(ByteRegister &reg, const Address &address)
{
    reg.set(busRead(address, OAMCorruption::ReadIncrease));
    hl.increment();
}

void CPU::opcodeLdi(const Address &address, const ByteRegister &reg)
{
    busWrite(address, reg.getValue());
    hl.increment();
}

// NOP
void CPU::opcodeNop()
{
    // Do nothing
    return;
}

// OR
void CPU::opcodeOrImpl(uint8_t value)
{
    uint8_t reg = a.getValue();
    uint8_t result = reg | value;

    a.set(result);

    setFlagZero(a.getValue() == 0);
    setFlagSubtract(false);
    setFlagHalfCarry(false);
    setFlagCarry(false);
}

void CPU::opcodeOr()
{
    opcodeOrImpl(getByteFromPC());
}

void CPU::opcodeOr(const ByteRegister &reg)
{
    opcodeOrImpl(reg.getValue());
}

void CPU::opcodeOr(const Address &address)
{
    opcodeOrImpl(busRead(address));
}

// POP
void CPU::opcodePop(RegisterPair &reg)
{
    stackPop(reg);
}

// PUSH
void CPU::opcodePush(const RegisterPair &reg)
{
    stackPush(reg);
}

// RES
void CPU::opcodeRes(uint8_t bit, ByteRegister &reg)
{
    uint8_t result = clearBit(reg.getValue(), bit);
    reg.set(result);
}

void CPU::opcodeRes(uint8_t bit, Address &&address)
{
    uint8_t value = busRead(address);
    uint8_t result = clearBit(value, bit);
    busWrite(address, result);
}

// RET
void CPU::opcodeRet()
{
    stackPop(pc);
}

// RET cc spends a cycle checking the condition before it pops the address
// https://gbdev.io/pandocs/CPU_Instruction_Set.html
void CPU::opcodeRet(Condition condition)
{
    internalCycle();
    if (isConditon(condition))
    {
        opcodeRet();
    }
}

// RETI returns and sets IME immediately, without EI's delay
// https://gbdev.io/pandocs/Interrupts.html#ime-interrupt-master-enable-flag-write-only
void CPU::opcodeReti()
{
    opcodeRet();
    isInterruptEnabled = true;
    imeEnableDelay = 0;
}

// RL
uint8_t CPU::opcodeRlImpl(uint8_t value)
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

void CPU::opcodeRl(ByteRegister &reg)
{
    uint8_t result = opcodeRlImpl(reg.getValue());
    reg.set(result);
}

void CPU::opcodeRl(Address &&address)
{
    uint8_t result = opcodeRlImpl(busRead(address));
    busWrite(address, result);
}

// RLA
void CPU::opcodeRla()
{
    opcodeRl(a);
    setFlagZero(false);
}

// RLC
uint8_t CPU::opcodeRlcImpl(uint8_t value)
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

void CPU::opcodeRlc(ByteRegister &reg)
{
    uint8_t result = opcodeRlcImpl(reg.getValue());
    reg.set(result);
}

void CPU::opcodeRlc(Address &&address)
{
    uint8_t result = opcodeRlcImpl(busRead(address));
    busWrite(address, result);
}

// RLCA
void CPU::opcodeRlca()
{
    opcodeRlc(a);
    setFlagZero(false);
}

// RR
uint8_t CPU::opcodeRrImpl(uint8_t value)
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

void CPU::opcodeRr(ByteRegister &reg)
{
    uint8_t result = opcodeRrImpl(reg.getValue());
    reg.set(result);
}

void CPU::opcodeRr(Address &&address)
{
    uint8_t result = opcodeRrImpl(busRead(address));
    busWrite(address, result);
}

// RRA
void CPU::opcodeRra()
{
    opcodeRr(a);
    setFlagZero(false);
}

// RRC
uint8_t CPU::opcodeRrcImpl(uint8_t value)
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

void CPU::opcodeRrc(ByteRegister &reg)
{
    uint8_t result = opcodeRrcImpl(reg.getValue());
    reg.set(result);
}

void CPU::opcodeRrc(Address &&address)
{
    uint8_t result = opcodeRrcImpl(busRead(address));
    busWrite(address, result);
}

// RRCA
void CPU::opcodeRrca()
{
    opcodeRrc(a);
    setFlagZero(false);
}

// RST
void CPU::opcodeRst(uint8_t offset)
{
    stackPush(pc);
    pc.set(offset);
}

// SBC
void CPU::opcodeSbcImpl(const uint8_t value)
{
    uint8_t carry = f.getCarryFlagValue();
    uint8_t reg = a.getValue();

    int32_t fullResult = reg - value - carry;
    uint8_t result = static_cast<uint8_t>(fullResult);

    setFlagZero(result == 0);
    setFlagSubtract(true);
    // Half carry: the low nibble needs a borrow from bit 4
    setFlagHalfCarry(((reg & 0xF) - (value & 0xF) - carry) < 0);
    setFlagCarry(fullResult < 0);

    a.set(result);
}

void CPU::opcodeSbc()
{
    opcodeSbcImpl(getByteFromPC());
}

void CPU::opcodeSbc(ByteRegister &reg)
{
    opcodeSbcImpl(reg.getValue());
}

void CPU::opcodeSbc(Address &&address)
{
    opcodeSbcImpl(busRead(address));
}

// SCF
void CPU::opcodeScf()
{
    setFlagSubtract(false);
    setFlagHalfCarry(false);
    setFlagCarry(true);
}

// SET
void CPU::opcodeSet(uint8_t bit, ByteRegister &reg)
{
    uint8_t result = setBit(reg.getValue(), bit);
    reg.set(result);
}

void CPU::opcodeSet(uint8_t bit, Address &&address)
{
    uint8_t value = busRead(address);
    uint8_t result = setBit(value, bit);
    busWrite(address, result);
}

// SLA
uint8_t CPU::opcodeSlaImpl(uint8_t value)
{
    uint8_t carryBit = checkBit(value, 7);
    uint8_t result = static_cast<uint8_t>(value << 1);

    setFlagZero(result == 0);
    setFlagSubtract(false);
    setFlagHalfCarry(false);
    setFlagCarry(carryBit);

    return result;
}

void CPU::opcodeSla(ByteRegister &reg)
{
    uint8_t result = opcodeSlaImpl(reg.getValue());
    reg.set(result);
}

void CPU::opcodeSla(Address &&address)
{
    uint8_t result = opcodeSlaImpl(busRead(address));
    busWrite(address, result);
}

// SRA
uint8_t CPU::opcodeSraImpl(uint8_t value)
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

void CPU::opcodeSra(ByteRegister &reg)
{
    uint8_t result = opcodeSraImpl(reg.getValue());
    reg.set(result);
}

void CPU::opcodeSra(Address &&address)
{
    uint8_t result = opcodeSraImpl(busRead(address));
    busWrite(address, result);
}

// SRL
uint8_t CPU::opcodeSrlImpl(uint8_t value)
{
    bool leastBitSet = checkBit(value, 0);
    uint8_t result = (value >> 1);

    setFlagZero(result == 0);
    setFlagSubtract(false);
    setFlagHalfCarry(false);
    setFlagCarry(leastBitSet);

    return result;
}

void CPU::opcodeSrl(ByteRegister &reg)
{
    uint8_t result = opcodeSrlImpl(reg.getValue());
    reg.set(result);
}

void CPU::opcodeSrl(Address &&address)
{
    uint8_t result = opcodeSrlImpl(busRead(address));
    busWrite(address, result);
}

// STOP
// https://gbdev.io/pandocs/Reducing_Power_Consumption.html#using-the-stop-instruction
// https://gbdev.io/pandocs/CGB_Registers.html#ff4d--key1spd-cgb-mode-only-prepare-speed-switch
void CPU::opcodeStop()
{
    getByteFromPC(); // STOP is followed by a padding byte.

    // STOP resets DIV
    emulator.timer.resetDivider();

    // On the CGB, STOP after preparing it through KEY1 switches speed
    if (emulator.cgbMode && emulator.speedSwitchPrepared)
    {
        emulator.doubleSpeed = !emulator.doubleSpeed;
        emulator.speedSwitchPrepared = false;
        emulator.speedSwitchPause = cpu::SPEED_SWITCH_PAUSE_CYCLES;
        return;
    }

    // Otherwise the system clock stops until a button is pressed
    isStopped = true;
}

// SUB
void CPU::opcodeSubImpl(uint8_t value)
{
    uint8_t reg = a.getValue();
    uint8_t result = static_cast<uint8_t>(reg - value);

    a.set(result);

    setFlagZero(a.getValue() == 0);
    setFlagSubtract(true);
    // Half carry: the low nibble needs a borrow from bit 4
    setFlagHalfCarry(((reg & 0xF) - (value & 0xF)) < 0);
    setFlagCarry(reg < value);
}

void CPU::opcodeSub()
{
    opcodeSubImpl(getByteFromPC());
}

void CPU::opcodeSub(ByteRegister &reg)
{
    opcodeSubImpl(reg.getValue());
}

void CPU::opcodeSub(Address &&address)
{
    opcodeSubImpl(busRead(address));
}

// SWAP
uint8_t CPU::opcodeSwapImpl(uint8_t value)
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

void CPU::opcodeSwap(ByteRegister &reg)
{
    uint8_t result = opcodeSwapImpl(reg.getValue());
    reg.set(result);
}

void CPU::opcodeSwap(Address &&address)
{
    uint8_t result = opcodeSwapImpl(busRead(address));
    busWrite(address, result);
}

// XOR
void CPU::opcodeXorImpl(uint8_t value)
{
    uint8_t reg = a.getValue();
    uint8_t result = reg ^ value;

    setFlagZero(result == 0);
    setFlagSubtract(false);
    setFlagHalfCarry(false);
    setFlagCarry(false);

    a.set(result);
}

void CPU::opcodeXor()
{
    opcodeXorImpl(getByteFromPC());
}

void CPU::opcodeXor(const ByteRegister &reg)
{
    opcodeXorImpl(reg.getValue());
}

void CPU::opcodeXor(const Address &address)
{
    opcodeXorImpl(busRead(address));
}
