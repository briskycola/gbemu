#include "CPU.hpp"
#include "Address.hpp"
#include "Emulator.hpp"
#include "OpcodeCycles.hpp"
#include "Common.hpp"
#include "CartridgeMetadata.hpp"

CPU::CPU(Emulator &emulator) :
    emulator(emulator),
    af(a, f),
    bc(b, c),
    de(d, e),
    hl(h, l)
{}

void CPU::initializeCGBPostBoot()
{
    // Values left by the CGB boot ROM when it hands control to a native
    // color cartridge at $0100. A = 0x11 is how games detect a CGB.
    af.set(0x1180);
    bc.set(0x0000);
    de.set(0xFF56);
    hl.set(0x000D);
    sp.set(0xFFFE);                  // Top of high RAM
    pc.set(ENTRY_POINT_START_ADDRESS);  // Cartridge entry point
    isInterruptEnabled = false;
    isHalted = false;
    haltBug = false;
    imeEnableDelay = 0;
}

Cycles CPU::executeOpcode(uint8_t opcode, uint16_t opcodePC)
{
    uint8_t cbOpcode;
    // We're executing a new instruction,
    // we're not taking any branches.
    branchTaken = false;

    // 0xCB is a special opcode indicating that
    // the Game Boy should go to the CB instruction table.
    if (opcode == OPCODE_CB_PREFIX)
    {
        cbOpcode = getByteFromPC();
        return executeCBOpcode(cbOpcode, opcodePC);
    }
    return executeNormalOpcode(opcode, opcodePC);
}

Cycles CPU::tick()
{
    if (emulator.dmaCyclesRemaining > 0)
    {
        // The CPU is stalled for one machine cycle
        // per remaining cycle of the DMA transfer
        --emulator.dmaCyclesRemaining;
        return Cycles(1);
    }

    // Take care of interrupts before next CPU tick
    if (handleInterrupts()) return Cycles(INTERRUPT_DISPATCH_CYCLES);

    if (isHalted) return 1;

    uint16_t opcodePC = pc.getValue();
    uint8_t opcode = getByteFromPC();
    Cycles cycles = executeOpcode(opcode, opcodePC);

    if (imeEnableDelay > 0)
    {
        --imeEnableDelay;
        if (imeEnableDelay == 0)
        {
            isInterruptEnabled = true;
        }
    }

    return cycles;
}

bool CPU::handleInterrupt(uint8_t interruptBit, uint16_t interruptVector, uint8_t firedInterrupts)
{
    if (!checkBit(firedInterrupts, interruptBit)) return false;

    interruptFlag.setBitTo(interruptBit, false);
    pc.set(interruptVector);
    isInterruptEnabled = false;
    return true;
}

bool CPU::handleInterrupts()
{
    const uint8_t firedInterrupts = interruptFlag.getValue() & interruptEnabled.getValue() & INTERRUPT_MASK;
    if (!firedInterrupts) return false;

    if (isHalted && firedInterrupts != 0x0)
    {
        isHalted = false;
    }

    if (!isInterruptEnabled)
    {
        return false;
    }

    stackPush(pc);

    // Handle all possible interrupts.
    if (handleInterrupt(INTERRUPT_VBLANK_BIT, INTERRUPT_VBLANK_VECTOR, firedInterrupts)) return true;

    if (handleInterrupt(INTERRUPT_LCD_STATUS_BIT, INTERRUPT_LCD_STATUS_VECTOR, firedInterrupts)) return true;

    if (handleInterrupt(INTERRUPT_TIMER_BIT, INTERRUPT_TIMER_VECTOR, firedInterrupts)) return true;

    if (handleInterrupt(INTERRUPT_SERIAL_BIT, INTERRUPT_SERIAL_VECTOR, firedInterrupts)) return true;

    return handleInterrupt(INTERRUPT_JOYPAD_BIT, INTERRUPT_JOYPAD_VECTOR, firedInterrupts);
}

uint8_t CPU::getByteFromPC()
{
    uint8_t byte = emulator.mmu.read(Address(pc));
    if (haltBug)
    {
        haltBug = false;
    }
    else
    {
        pc.increment();
    }
    return byte;
}

int8_t CPU::getSignedByteFromPC()
{
    uint8_t byte = getByteFromPC();
    return static_cast<int8_t>(byte);
}

uint16_t CPU::getWordFromPC()
{
    uint8_t lowByte = getByteFromPC();
    uint8_t highByte = getByteFromPC();
    return composeBytes(highByte, lowByte);
}

void CPU::setFlagZero(bool set)
{
    f.setZeroFlag(set);
}

void CPU::setFlagSubtract(bool set)
{
    f.setSubtractionFlag(set);
}

void CPU::setFlagHalfCarry(bool set)
{
    f.setHalfCarryFlag(set);
}

void CPU::setFlagCarry(bool set)
{
    f.setCarryFlag(set);
}

bool CPU::isConditon(Condition condition)
{
    bool shouldBranch;

    switch (condition)
    {
        case Condition::C:
            shouldBranch = f.carryFlag();
            break;
        case Condition::NC:
            shouldBranch = !f.carryFlag();
            break;
        case Condition::Z:
            shouldBranch = f.zeroFlag();
            break;
        case Condition::NZ:
            shouldBranch = !f.zeroFlag();
            break;
    }

    // If the branch is taken, we need to remember
    // so the correct CPU cycles can be used
    branchTaken = shouldBranch;
    return shouldBranch;
}

void CPU::stackPush(const WordValue &reg)
{
    sp.decrement();
    emulator.mmu.write(Address(sp), reg.getHighByte());
    sp.decrement();
    emulator.mmu.write(Address(sp), reg.getLowByte());
}

void CPU::stackPop(WordValue &reg)
{
    uint8_t lowByte = emulator.mmu.read(Address(sp));
    sp.increment();
    uint8_t highByte = emulator.mmu.read(Address(sp));
    sp.increment();

    uint16_t value = composeBytes(highByte, lowByte);
    reg.set(value);
}

Cycles CPU::executeNormalOpcode(uint8_t opcode, uint16_t opcodePC)
{
    //std::cout << "0x"
    //      << std::uppercase << std::hex
    //      << opcodePC << ": " << OPCODE_NAMES[opcode] << "\n";
    switch (opcode)
    {
        case 0x00: opcode00(); break; case 0x01: opcode01(); break; case 0x02: opcode02(); break; case 0x03: opcode03(); break; case 0x04: opcode04(); break; case 0x05: opcode05(); break; case 0x06: opcode06(); break; case 0x07: opcode07(); break; case 0x08: opcode08(); break; case 0x09: opcode09(); break; case 0x0A: opcode0A(); break; case 0x0B: opcode0B(); break; case 0x0C: opcode0C(); break; case 0x0D: opcode0D(); break; case 0x0E: opcode0E(); break; case 0x0F: opcode0F(); break;
        case 0x10: opcode10(); break; case 0x11: opcode11(); break; case 0x12: opcode12(); break; case 0x13: opcode13(); break; case 0x14: opcode14(); break; case 0x15: opcode15(); break; case 0x16: opcode16(); break; case 0x17: opcode17(); break; case 0x18: opcode18(); break; case 0x19: opcode19(); break; case 0x1A: opcode1A(); break; case 0x1B: opcode1B(); break; case 0x1C: opcode1C(); break; case 0x1D: opcode1D(); break; case 0x1E: opcode1E(); break; case 0x1F: opcode1F(); break;
        case 0x20: opcode20(); break; case 0x21: opcode21(); break; case 0x22: opcode22(); break; case 0x23: opcode23(); break; case 0x24: opcode24(); break; case 0x25: opcode25(); break; case 0x26: opcode26(); break; case 0x27: opcode27(); break; case 0x28: opcode28(); break; case 0x29: opcode29(); break; case 0x2A: opcode2A(); break; case 0x2B: opcode2B(); break; case 0x2C: opcode2C(); break; case 0x2D: opcode2D(); break; case 0x2E: opcode2E(); break; case 0x2F: opcode2F(); break;
        case 0x30: opcode30(); break; case 0x31: opcode31(); break; case 0x32: opcode32(); break; case 0x33: opcode33(); break; case 0x34: opcode34(); break; case 0x35: opcode35(); break; case 0x36: opcode36(); break; case 0x37: opcode37(); break; case 0x38: opcode38(); break; case 0x39: opcode39(); break; case 0x3A: opcode3A(); break; case 0x3B: opcode3B(); break; case 0x3C: opcode3C(); break; case 0x3D: opcode3D(); break; case 0x3E: opcode3E(); break; case 0x3F: opcode3F(); break;
        case 0x40: opcode40(); break; case 0x41: opcode41(); break; case 0x42: opcode42(); break; case 0x43: opcode43(); break; case 0x44: opcode44(); break; case 0x45: opcode45(); break; case 0x46: opcode46(); break; case 0x47: opcode47(); break; case 0x48: opcode48(); break; case 0x49: opcode49(); break; case 0x4A: opcode4A(); break; case 0x4B: opcode4B(); break; case 0x4C: opcode4C(); break; case 0x4D: opcode4D(); break; case 0x4E: opcode4E(); break; case 0x4F: opcode4F(); break;
        case 0x50: opcode50(); break; case 0x51: opcode51(); break; case 0x52: opcode52(); break; case 0x53: opcode53(); break; case 0x54: opcode54(); break; case 0x55: opcode55(); break; case 0x56: opcode56(); break; case 0x57: opcode57(); break; case 0x58: opcode58(); break; case 0x59: opcode59(); break; case 0x5A: opcode5A(); break; case 0x5B: opcode5B(); break; case 0x5C: opcode5C(); break; case 0x5D: opcode5D(); break; case 0x5E: opcode5E(); break; case 0x5F: opcode5F(); break;
        case 0x60: opcode60(); break; case 0x61: opcode61(); break; case 0x62: opcode62(); break; case 0x63: opcode63(); break; case 0x64: opcode64(); break; case 0x65: opcode65(); break; case 0x66: opcode66(); break; case 0x67: opcode67(); break; case 0x68: opcode68(); break; case 0x69: opcode69(); break; case 0x6A: opcode6A(); break; case 0x6B: opcode6B(); break; case 0x6C: opcode6C(); break; case 0x6D: opcode6D(); break; case 0x6E: opcode6E(); break; case 0x6F: opcode6F(); break;
        case 0x70: opcode70(); break; case 0x71: opcode71(); break; case 0x72: opcode72(); break; case 0x73: opcode73(); break; case 0x74: opcode74(); break; case 0x75: opcode75(); break; case 0x76: opcode76(); break; case 0x77: opcode77(); break; case 0x78: opcode78(); break; case 0x79: opcode79(); break; case 0x7A: opcode7A(); break; case 0x7B: opcode7B(); break; case 0x7C: opcode7C(); break; case 0x7D: opcode7D(); break; case 0x7E: opcode7E(); break; case 0x7F: opcode7F(); break;
        case 0x80: opcode80(); break; case 0x81: opcode81(); break; case 0x82: opcode82(); break; case 0x83: opcode83(); break; case 0x84: opcode84(); break; case 0x85: opcode85(); break; case 0x86: opcode86(); break; case 0x87: opcode87(); break; case 0x88: opcode88(); break; case 0x89: opcode89(); break; case 0x8A: opcode8A(); break; case 0x8B: opcode8B(); break; case 0x8C: opcode8C(); break; case 0x8D: opcode8D(); break; case 0x8E: opcode8E(); break; case 0x8F: opcode8F(); break;
        case 0x90: opcode90(); break; case 0x91: opcode91(); break; case 0x92: opcode92(); break; case 0x93: opcode93(); break; case 0x94: opcode94(); break; case 0x95: opcode95(); break; case 0x96: opcode96(); break; case 0x97: opcode97(); break; case 0x98: opcode98(); break; case 0x99: opcode99(); break; case 0x9A: opcode9A(); break; case 0x9B: opcode9B(); break; case 0x9C: opcode9C(); break; case 0x9D: opcode9D(); break; case 0x9E: opcode9E(); break; case 0x9F: opcode9F(); break;
        case 0xA0: opcodeA0(); break; case 0xA1: opcodeA1(); break; case 0xA2: opcodeA2(); break; case 0xA3: opcodeA3(); break; case 0xA4: opcodeA4(); break; case 0xA5: opcodeA5(); break; case 0xA6: opcodeA6(); break; case 0xA7: opcodeA7(); break; case 0xA8: opcodeA8(); break; case 0xA9: opcodeA9(); break; case 0xAA: opcodeAA(); break; case 0xAB: opcodeAB(); break; case 0xAC: opcodeAC(); break; case 0xAD: opcodeAD(); break; case 0xAE: opcodeAE(); break; case 0xAF: opcodeAF(); break;
        case 0xB0: opcodeB0(); break; case 0xB1: opcodeB1(); break; case 0xB2: opcodeB2(); break; case 0xB3: opcodeB3(); break; case 0xB4: opcodeB4(); break; case 0xB5: opcodeB5(); break; case 0xB6: opcodeB6(); break; case 0xB7: opcodeB7(); break; case 0xB8: opcodeB8(); break; case 0xB9: opcodeB9(); break; case 0xBA: opcodeBA(); break; case 0xBB: opcodeBB(); break; case 0xBC: opcodeBC(); break; case 0xBD: opcodeBD(); break; case 0xBE: opcodeBE(); break; case 0xBF: opcodeBF(); break;
        case 0xC0: opcodeC0(); break; case 0xC1: opcodeC1(); break; case 0xC2: opcodeC2(); break; case 0xC3: opcodeC3(); break; case 0xC4: opcodeC4(); break; case 0xC5: opcodeC5(); break; case 0xC6: opcodeC6(); break; case 0xC7: opcodeC7(); break; case 0xC8: opcodeC8(); break; case 0xC9: opcodeC9(); break; case 0xCA: opcodeCA(); break; case 0xCB: opcodeCB(); break; case 0xCC: opcodeCC(); break; case 0xCD: opcodeCD(); break; case 0xCE: opcodeCE(); break; case 0xCF: opcodeCF(); break;
        case 0xD0: opcodeD0(); break; case 0xD1: opcodeD1(); break; case 0xD2: opcodeD2(); break; case 0xD3: opcodeD3(); break; case 0xD4: opcodeD4(); break; case 0xD5: opcodeD5(); break; case 0xD6: opcodeD6(); break; case 0xD7: opcodeD7(); break; case 0xD8: opcodeD8(); break; case 0xD9: opcodeD9(); break; case 0xDA: opcodeDA(); break; case 0xDB: opcodeDB(); break; case 0xDC: opcodeDC(); break; case 0xDD: opcodeDD(); break; case 0xDE: opcodeDE(); break; case 0xDF: opcodeDF(); break;
        case 0xE0: opcodeE0(); break; case 0xE1: opcodeE1(); break; case 0xE2: opcodeE2(); break; case 0xE3: opcodeE3(); break; case 0xE4: opcodeE4(); break; case 0xE5: opcodeE5(); break; case 0xE6: opcodeE6(); break; case 0xE7: opcodeE7(); break; case 0xE8: opcodeE8(); break; case 0xE9: opcodeE9(); break; case 0xEA: opcodeEA(); break; case 0xEB: opcodeEB(); break; case 0xEC: opcodeEC(); break; case 0xED: opcodeED(); break; case 0xEE: opcodeEE(); break; case 0xEF: opcodeEF(); break;
        case 0xF0: opcodeF0(); break; case 0xF1: opcodeF1(); break; case 0xF2: opcodeF2(); break; case 0xF3: opcodeF3(); break; case 0xF4: opcodeF4(); break; case 0xF5: opcodeF5(); break; case 0xF6: opcodeF6(); break; case 0xF7: opcodeF7(); break; case 0xF8: opcodeF8(); break; case 0xF9: opcodeF9(); break; case 0xFA: opcodeFA(); break; case 0xFB: opcodeFB(); break; case 0xFC: opcodeFC(); break; case 0xFD: opcodeFD(); break; case 0xFE: opcodeFE(); break; case 0xFF: opcodeFF(); break;
    }
    return !branchTaken ? OPCODE_CYCLES[opcode] : OPCODE_CYCLES_BRANCHED[opcode];
}

Cycles CPU::executeCBOpcode(const uint8_t opcode, uint16_t opcodePC)
{
    //std::cout << "0x"
    //      << std::uppercase << std::hex
    //      << opcodePC << ": " << OPCODE_CB_NAMES[opcode] << "\n";
    switch (opcode)
    {
        case 0x00: opcodeCB00(); break; case 0x01: opcodeCB01(); break; case 0x02: opcodeCB02(); break; case 0x03: opcodeCB03(); break; case 0x04: opcodeCB04(); break; case 0x05: opcodeCB05(); break; case 0x06: opcodeCB06(); break; case 0x07: opcodeCB07(); break; case 0x08: opcodeCB08(); break; case 0x09: opcodeCB09(); break; case 0x0A: opcodeCB0A(); break; case 0x0B: opcodeCB0B(); break; case 0x0C: opcodeCB0C(); break; case 0x0D: opcodeCB0D(); break; case 0x0E: opcodeCB0E(); break; case 0x0F: opcodeCB0F(); break;
        case 0x10: opcodeCB10(); break; case 0x11: opcodeCB11(); break; case 0x12: opcodeCB12(); break; case 0x13: opcodeCB13(); break; case 0x14: opcodeCB14(); break; case 0x15: opcodeCB15(); break; case 0x16: opcodeCB16(); break; case 0x17: opcodeCB17(); break; case 0x18: opcodeCB18(); break; case 0x19: opcodeCB19(); break; case 0x1A: opcodeCB1A(); break; case 0x1B: opcodeCB1B(); break; case 0x1C: opcodeCB1C(); break; case 0x1D: opcodeCB1D(); break; case 0x1E: opcodeCB1E(); break; case 0x1F: opcodeCB1F(); break;
        case 0x20: opcodeCB20(); break; case 0x21: opcodeCB21(); break; case 0x22: opcodeCB22(); break; case 0x23: opcodeCB23(); break; case 0x24: opcodeCB24(); break; case 0x25: opcodeCB25(); break; case 0x26: opcodeCB26(); break; case 0x27: opcodeCB27(); break; case 0x28: opcodeCB28(); break; case 0x29: opcodeCB29(); break; case 0x2A: opcodeCB2A(); break; case 0x2B: opcodeCB2B(); break; case 0x2C: opcodeCB2C(); break; case 0x2D: opcodeCB2D(); break; case 0x2E: opcodeCB2E(); break; case 0x2F: opcodeCB2F(); break;
        case 0x30: opcodeCB30(); break; case 0x31: opcodeCB31(); break; case 0x32: opcodeCB32(); break; case 0x33: opcodeCB33(); break; case 0x34: opcodeCB34(); break; case 0x35: opcodeCB35(); break; case 0x36: opcodeCB36(); break; case 0x37: opcodeCB37(); break; case 0x38: opcodeCB38(); break; case 0x39: opcodeCB39(); break; case 0x3A: opcodeCB3A(); break; case 0x3B: opcodeCB3B(); break; case 0x3C: opcodeCB3C(); break; case 0x3D: opcodeCB3D(); break; case 0x3E: opcodeCB3E(); break; case 0x3F: opcodeCB3F(); break;
        case 0x40: opcodeCB40(); break; case 0x41: opcodeCB41(); break; case 0x42: opcodeCB42(); break; case 0x43: opcodeCB43(); break; case 0x44: opcodeCB44(); break; case 0x45: opcodeCB45(); break; case 0x46: opcodeCB46(); break; case 0x47: opcodeCB47(); break; case 0x48: opcodeCB48(); break; case 0x49: opcodeCB49(); break; case 0x4A: opcodeCB4A(); break; case 0x4B: opcodeCB4B(); break; case 0x4C: opcodeCB4C(); break; case 0x4D: opcodeCB4D(); break; case 0x4E: opcodeCB4E(); break; case 0x4F: opcodeCB4F(); break;
        case 0x50: opcodeCB50(); break; case 0x51: opcodeCB51(); break; case 0x52: opcodeCB52(); break; case 0x53: opcodeCB53(); break; case 0x54: opcodeCB54(); break; case 0x55: opcodeCB55(); break; case 0x56: opcodeCB56(); break; case 0x57: opcodeCB57(); break; case 0x58: opcodeCB58(); break; case 0x59: opcodeCB59(); break; case 0x5A: opcodeCB5A(); break; case 0x5B: opcodeCB5B(); break; case 0x5C: opcodeCB5C(); break; case 0x5D: opcodeCB5D(); break; case 0x5E: opcodeCB5E(); break; case 0x5F: opcodeCB5F(); break;
        case 0x60: opcodeCB60(); break; case 0x61: opcodeCB61(); break; case 0x62: opcodeCB62(); break; case 0x63: opcodeCB63(); break; case 0x64: opcodeCB64(); break; case 0x65: opcodeCB65(); break; case 0x66: opcodeCB66(); break; case 0x67: opcodeCB67(); break; case 0x68: opcodeCB68(); break; case 0x69: opcodeCB69(); break; case 0x6A: opcodeCB6A(); break; case 0x6B: opcodeCB6B(); break; case 0x6C: opcodeCB6C(); break; case 0x6D: opcodeCB6D(); break; case 0x6E: opcodeCB6E(); break; case 0x6F: opcodeCB6F(); break;
        case 0x70: opcodeCB70(); break; case 0x71: opcodeCB71(); break; case 0x72: opcodeCB72(); break; case 0x73: opcodeCB73(); break; case 0x74: opcodeCB74(); break; case 0x75: opcodeCB75(); break; case 0x76: opcodeCB76(); break; case 0x77: opcodeCB77(); break; case 0x78: opcodeCB78(); break; case 0x79: opcodeCB79(); break; case 0x7A: opcodeCB7A(); break; case 0x7B: opcodeCB7B(); break; case 0x7C: opcodeCB7C(); break; case 0x7D: opcodeCB7D(); break; case 0x7E: opcodeCB7E(); break; case 0x7F: opcodeCB7F(); break;
        case 0x80: opcodeCB80(); break; case 0x81: opcodeCB81(); break; case 0x82: opcodeCB82(); break; case 0x83: opcodeCB83(); break; case 0x84: opcodeCB84(); break; case 0x85: opcodeCB85(); break; case 0x86: opcodeCB86(); break; case 0x87: opcodeCB87(); break; case 0x88: opcodeCB88(); break; case 0x89: opcodeCB89(); break; case 0x8A: opcodeCB8A(); break; case 0x8B: opcodeCB8B(); break; case 0x8C: opcodeCB8C(); break; case 0x8D: opcodeCB8D(); break; case 0x8E: opcodeCB8E(); break; case 0x8F: opcodeCB8F(); break;
        case 0x90: opcodeCB90(); break; case 0x91: opcodeCB91(); break; case 0x92: opcodeCB92(); break; case 0x93: opcodeCB93(); break; case 0x94: opcodeCB94(); break; case 0x95: opcodeCB95(); break; case 0x96: opcodeCB96(); break; case 0x97: opcodeCB97(); break; case 0x98: opcodeCB98(); break; case 0x99: opcodeCB99(); break; case 0x9A: opcodeCB9A(); break; case 0x9B: opcodeCB9B(); break; case 0x9C: opcodeCB9C(); break; case 0x9D: opcodeCB9D(); break; case 0x9E: opcodeCB9E(); break; case 0x9F: opcodeCB9F(); break;
        case 0xA0: opcodeCBA0(); break; case 0xA1: opcodeCBA1(); break; case 0xA2: opcodeCBA2(); break; case 0xA3: opcodeCBA3(); break; case 0xA4: opcodeCBA4(); break; case 0xA5: opcodeCBA5(); break; case 0xA6: opcodeCBA6(); break; case 0xA7: opcodeCBA7(); break; case 0xA8: opcodeCBA8(); break; case 0xA9: opcodeCBA9(); break; case 0xAA: opcodeCBAA(); break; case 0xAB: opcodeCBAB(); break; case 0xAC: opcodeCBAC(); break; case 0xAD: opcodeCBAD(); break; case 0xAE: opcodeCBAE(); break; case 0xAF: opcodeCBAF(); break;
        case 0xB0: opcodeCBB0(); break; case 0xB1: opcodeCBB1(); break; case 0xB2: opcodeCBB2(); break; case 0xB3: opcodeCBB3(); break; case 0xB4: opcodeCBB4(); break; case 0xB5: opcodeCBB5(); break; case 0xB6: opcodeCBB6(); break; case 0xB7: opcodeCBB7(); break; case 0xB8: opcodeCBB8(); break; case 0xB9: opcodeCBB9(); break; case 0xBA: opcodeCBBA(); break; case 0xBB: opcodeCBBB(); break; case 0xBC: opcodeCBBC(); break; case 0xBD: opcodeCBBD(); break; case 0xBE: opcodeCBBE(); break; case 0xBF: opcodeCBBF(); break;
        case 0xC0: opcodeCBC0(); break; case 0xC1: opcodeCBC1(); break; case 0xC2: opcodeCBC2(); break; case 0xC3: opcodeCBC3(); break; case 0xC4: opcodeCBC4(); break; case 0xC5: opcodeCBC5(); break; case 0xC6: opcodeCBC6(); break; case 0xC7: opcodeCBC7(); break; case 0xC8: opcodeCBC8(); break; case 0xC9: opcodeCBC9(); break; case 0xCA: opcodeCBCA(); break; case 0xCB: opcodeCBCB(); break; case 0xCC: opcodeCBCC(); break; case 0xCD: opcodeCBCD(); break; case 0xCE: opcodeCBCE(); break; case 0xCF: opcodeCBCF(); break;
        case 0xD0: opcodeCBD0(); break; case 0xD1: opcodeCBD1(); break; case 0xD2: opcodeCBD2(); break; case 0xD3: opcodeCBD3(); break; case 0xD4: opcodeCBD4(); break; case 0xD5: opcodeCBD5(); break; case 0xD6: opcodeCBD6(); break; case 0xD7: opcodeCBD7(); break; case 0xD8: opcodeCBD8(); break; case 0xD9: opcodeCBD9(); break; case 0xDA: opcodeCBDA(); break; case 0xDB: opcodeCBDB(); break; case 0xDC: opcodeCBDC(); break; case 0xDD: opcodeCBDD(); break; case 0xDE: opcodeCBDE(); break; case 0xDF: opcodeCBDF(); break;
        case 0xE0: opcodeCBE0(); break; case 0xE1: opcodeCBE1(); break; case 0xE2: opcodeCBE2(); break; case 0xE3: opcodeCBE3(); break; case 0xE4: opcodeCBE4(); break; case 0xE5: opcodeCBE5(); break; case 0xE6: opcodeCBE6(); break; case 0xE7: opcodeCBE7(); break; case 0xE8: opcodeCBE8(); break; case 0xE9: opcodeCBE9(); break; case 0xEA: opcodeCBEA(); break; case 0xEB: opcodeCBEB(); break; case 0xEC: opcodeCBEC(); break; case 0xED: opcodeCBED(); break; case 0xEE: opcodeCBEE(); break; case 0xEF: opcodeCBEF(); break;
        case 0xF0: opcodeCBF0(); break; case 0xF1: opcodeCBF1(); break; case 0xF2: opcodeCBF2(); break; case 0xF3: opcodeCBF3(); break; case 0xF4: opcodeCBF4(); break; case 0xF5: opcodeCBF5(); break; case 0xF6: opcodeCBF6(); break; case 0xF7: opcodeCBF7(); break; case 0xF8: opcodeCBF8(); break; case 0xF9: opcodeCBF9(); break; case 0xFA: opcodeCBFA(); break; case 0xFB: opcodeCBFB(); break; case 0xFC: opcodeCBFC(); break; case 0xFD: opcodeCBFD(); break; case 0xFE: opcodeCBFE(); break; case 0xFF: opcodeCBFF(); break;
    }
    return OPCODE_CYCLES_CB[opcode];
}
