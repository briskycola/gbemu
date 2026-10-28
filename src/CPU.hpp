#pragma once

#include "Address.hpp"
#include "Register.hpp"
#include <cstdint>

// https://gbdev.io/pandocs/CPU_Registers_and_Flags.html
// https://gbdev.io/pandocs/CPU_Instruction_Set.html
// https://gbdev.io/pandocs/Interrupts.html

class Emulator;

enum class Condition
{
    NZ,
    Z,
    NC,
    C,
};

// RST
static constexpr uint16_t RST1 = 0x00;
static constexpr uint16_t RST2 = 0x08;
static constexpr uint16_t RST3 = 0x10;
static constexpr uint16_t RST4 = 0x18;
static constexpr uint16_t RST5 = 0x20;
static constexpr uint16_t RST6 = 0x28;
static constexpr uint16_t RST7 = 0x30;
static constexpr uint16_t RST8 = 0x38;

// Addresses the CPU jumps to when servicing each interrupt
static constexpr uint16_t INTERRUPT_VBLANK_VECTOR    = 0x40;
static constexpr uint16_t INTERRUPT_LCD_STATUS_VECTOR = 0x48;
static constexpr uint16_t INTERRUPT_TIMER_VECTOR     = 0x50;
static constexpr uint16_t INTERRUPT_SERIAL_VECTOR    = 0x58;
static constexpr uint16_t INTERRUPT_JOYPAD_VECTOR    = 0x60;

// Bit of each interrupt in the IF and IE registers.
// Lower bits have higher priority.
static constexpr uint8_t INTERRUPT_VBLANK_BIT    = 0;
static constexpr uint8_t INTERRUPT_LCD_STATUS_BIT = 1;
static constexpr uint8_t INTERRUPT_TIMER_BIT     = 2;
static constexpr uint8_t INTERRUPT_SERIAL_BIT    = 3;
static constexpr uint8_t INTERRUPT_JOYPAD_BIT    = 4;

// Only the low 5 bits of IF and IE are used; the
// upper 3 bits always read back as 1
static constexpr uint8_t INTERRUPT_MASK = 0x1F;

// Servicing an interrupt takes 5 machine cycles
static constexpr uint32_t INTERRUPT_DISPATCH_CYCLES = 5;

// EI enables interrupts only after the instruction
// following it, so the enable is delayed by 2 steps
static constexpr uint8_t EI_ENABLE_DELAY = 2;

static constexpr uint16_t OPCODE_CB_PREFIX = 0xCB;

class CPU
{
    private:
        Emulator &emulator;
        bool isInterruptEnabled = false;
        bool isHalted = false;
        bool haltBug = false;
        uint8_t imeEnableDelay = 0;
        bool branchTaken = false;

        // CPU Registers
        ByteRegister a, b, c, d, e, h, l;

        // Combined registers when addressing
        // a 16-bit value
        RegisterPair af, bc, de, hl;

        // Flag Register for CPU Flags
        //
        // Possible values for Flag Register
        // depending on the last instruction
        //
        // 0x80 - produced 0
        // 0x40 - was a subtraction operation
        // 0x20 - lower half of the byte overflowed 15
        // 0x10 - overflowed 255 or underflowed 0 for additions/subtractions
        FlagRegister f;

        // Program Counter
        WordRegister pc;

        // Stack Pointer
        WordRegister sp;

        bool handleInterrupts();
        bool handleInterrupt(uint8_t interruptBit, uint16_t interruptVector, uint8_t firedInterrupts);

        void setFlagZero(bool set);
        void setFlagSubtract(bool set);
        void setFlagHalfCarry(bool set);
        void setFlagCarry(bool set);

        bool isConditon(Condition condition);

        uint8_t getByteFromPC();
        int8_t getSignedByteFromPC();
        uint16_t getWordFromPC();

        void stackPush(const WordValue &reg);
        void stackPop(WordValue &reg);

        // Instruction prototypes

        // ADC
        void opcodeAdcImpl(uint8_t value);
        void opcodeAdc();
        void opcodeAdc(const ByteRegister &reg);
        void opcodeAdc(const Address &&address);

        // ADD
        void opcodeAddImpl(uint8_t reg, uint8_t value);

        void opcodeAddA();
        void opcodeAddA(const ByteRegister &reg);
        void opcodeAddA(const Address &address);

        void opcodeAddHLImpl(uint16_t value);
        void opcodeAddHL(const RegisterPair &regPair);
        void opcodeAddHL(const WordRegister &wordReg);

        void opcodeAddSP();
        void opcodeAddSigned();

        // AND
        void opcodeAndImpl(uint8_t value);
        void opcodeAnd();
        void opcodeAnd(ByteRegister &reg);
        void opcodeAnd(Address &&address);

        // BIT
        void opcodeBitImpl(uint8_t bit, uint8_t value);
        void opcodeBit(uint8_t bit, ByteRegister &reg);
        void opcodeBit(uint8_t bit, Address &&address);

        // CALL
        void opcodeCall();
        void opcodeCall(Condition condition);

        // CCF
        void opcodeCcf();
        
        // CP
        void opcodeCpImpl(uint8_t value);
        void opcodeCp();
        void opcodeCp(const ByteRegister &reg);
        void opcodeCp(const Address &address);

        // CPL
        void opcodeCpl();

        // DAA
        void opcodeDaa();

        // DEC
        void opcodeDec(ByteRegister &reg);
        void opcodeDec(RegisterPair &reg);
        void opcodeDec(WordRegister &reg);
        void opcodeDec(Address &&address);

        // DI
        void opcodeDi();

        // EI
        void opcodeEi();

        // INC
        void opcodeInc(ByteRegister &reg);
        void opcodeInc(RegisterPair &reg);
        void opcodeInc(WordRegister &reg);
        void opcodeInc(Address &&address);

        // JP
        void opcodeJp();
        void opcodeJp(Condition condition);
        void opcodeJp(const Address &address);

        // JR
        void opcodeJr();
        void opcodeJr(Condition condition);

        // HALT
        void opcodeHalt();

        // LD
        void opcodeLd(ByteRegister &reg);
        void opcodeLd(ByteRegister &reg, const ByteRegister &byteReg);
        void opcodeLd(ByteRegister &reg, const Address &address);
        void opcodeLd(RegisterPair &reg);
        void opcodeLd(WordRegister &reg);
        void opcodeLd(WordRegister &reg, const RegisterPair &regPair);
        void opcodeLd(const Address &address);
        void opcodeLd(const Address &address, const ByteRegister &byteReg);
        void opcodeLd(const Address &address, const WordRegister &wordReg);

        // (nn), A
        void opcodeLdToAddr(const ByteRegister &reg);
        void opcodeLdFromAddr(ByteRegister &reg);

        // LDD
        void opcodeLdd(ByteRegister &reg, const Address &address);
        void opcodeLdd(const Address &address, const ByteRegister &reg);

        // LDH
        // A, (n)
        void opcodeLdhIntoA();
        // (n), A
        void opcodeLdhIntoData();
        // (reg), A
        void opcodeLdhIntoC();
        // A, (reg)
        void opcodeLdhCIntoA();

        // LDHL
        void opcodeLdhl();

        // LDI
        void opcodeLdi(ByteRegister &reg, const Address &address);
        void opcodeLdi(const Address &address, const ByteRegister &reg);

        // NOP
        void opcodeNop();

        // OR
        void opcodeOrImpl(uint8_t value);
        void opcodeOr();
        void opcodeOr(const ByteRegister &reg);
        void opcodeOr(const Address &addr);

        // POP
        void opcodePop(RegisterPair &reg);

        // PUSH
        void opcodePush(const RegisterPair &reg);

        // RES
        void opcodeRes(uint8_t bit, ByteRegister &reg);
        void opcodeRes(uint8_t bit, Address &&address);

        // RET
        void opcodeRet();
        void opcodeRet(Condition condition);

        // RETI
        void opcodeReti();

        // RL
        uint8_t opcodeRlImpl(uint8_t value);
        void opcodeRl(ByteRegister &reg);
        void opcodeRl(Address &&address);

        // RLA
        void opcodeRla();

        // RLC
        uint8_t opcodeRlcImpl(uint8_t value);
        void opcodeRlc(ByteRegister &reg);
        void opcodeRlc(Address &&address);

        // RLCA
        void opcodeRlca();

        // RR
        uint8_t opcodeRrImpl(uint8_t value);
        void opcodeRr(ByteRegister &reg);
        void opcodeRr(Address &&address);

        // RRA
        void opcodeRra();

        // RRC
        uint8_t opcodeRrcImpl(uint8_t value);
        void opcodeRrc(ByteRegister &reg);
        void opcodeRrc(Address &&address);

        // RRCA
        void opcodeRrca();

        // RST
        void opcodeRst(uint8_t offset);

        // SBC
        void opcodeSbcImpl(const uint8_t value);
        void opcodeSbc();
        void opcodeSbc(ByteRegister &reg);
        void opcodeSbc(Address &&address);

        // SCF
        void opcodeScf();

        // SET
        void opcodeSet(uint8_t bit, ByteRegister &reg);
        void opcodeSet(uint8_t bit, Address &&address);

        // SLA
        uint8_t opcodeSlaImpl(uint8_t value);
        void opcodeSla(ByteRegister &reg);
        void opcodeSla(Address &&address);

        // SRA
        uint8_t opcodeSraImpl(uint8_t value);
        void opcodeSra(ByteRegister &reg);
        void opcodeSra(Address &&address);

        // SRL
        uint8_t opcodeSrlImpl(uint8_t value);
        void opcodeSrl(ByteRegister &reg);
        void opcodeSrl(Address &&address);

        // STOP
        void opcodeStop();

        // SUB
        void opcodeSubImpl(uint8_t value);
        void opcodeSub();
        void opcodeSub(ByteRegister &reg);
        void opcodeSub(Address &&address);

        // SWAP
        uint8_t opcodeSwapImpl(uint8_t value);
        void opcodeSwap(ByteRegister &reg);
        void opcodeSwap(Address &&address);

        // XOR
        void opcodeXorImpl(uint8_t value);
        void opcodeXor();
        void opcodeXor(const ByteRegister &reg);
        void opcodeXor(const Address &address);

        // Opcodes
        void opcode00(); void opcode01(); void opcode02(); void opcode03(); void opcode04(); void opcode05(); void opcode06(); void opcode07(); void opcode08(); void opcode09(); void opcode0A(); void opcode0B(); void opcode0C(); void opcode0D(); void opcode0E(); void opcode0F();
        void opcode10(); void opcode11(); void opcode12(); void opcode13(); void opcode14(); void opcode15(); void opcode16(); void opcode17(); void opcode18(); void opcode19(); void opcode1A(); void opcode1B(); void opcode1C(); void opcode1D(); void opcode1E(); void opcode1F();
        void opcode20(); void opcode21(); void opcode22(); void opcode23(); void opcode24(); void opcode25(); void opcode26(); void opcode27(); void opcode28(); void opcode29(); void opcode2A(); void opcode2B(); void opcode2C(); void opcode2D(); void opcode2E(); void opcode2F();
        void opcode30(); void opcode31(); void opcode32(); void opcode33(); void opcode34(); void opcode35(); void opcode36(); void opcode37(); void opcode38(); void opcode39(); void opcode3A(); void opcode3B(); void opcode3C(); void opcode3D(); void opcode3E(); void opcode3F();
        void opcode40(); void opcode41(); void opcode42(); void opcode43(); void opcode44(); void opcode45(); void opcode46(); void opcode47(); void opcode48(); void opcode49(); void opcode4A(); void opcode4B(); void opcode4C(); void opcode4D(); void opcode4E(); void opcode4F();
        void opcode50(); void opcode51(); void opcode52(); void opcode53(); void opcode54(); void opcode55(); void opcode56(); void opcode57(); void opcode58(); void opcode59(); void opcode5A(); void opcode5B(); void opcode5C(); void opcode5D(); void opcode5E(); void opcode5F();
        void opcode60(); void opcode61(); void opcode62(); void opcode63(); void opcode64(); void opcode65(); void opcode66(); void opcode67(); void opcode68(); void opcode69(); void opcode6A(); void opcode6B(); void opcode6C(); void opcode6D(); void opcode6E(); void opcode6F();
        void opcode70(); void opcode71(); void opcode72(); void opcode73(); void opcode74(); void opcode75(); void opcode76(); void opcode77(); void opcode78(); void opcode79(); void opcode7A(); void opcode7B(); void opcode7C(); void opcode7D(); void opcode7E(); void opcode7F();
        void opcode80(); void opcode81(); void opcode82(); void opcode83(); void opcode84(); void opcode85(); void opcode86(); void opcode87(); void opcode88(); void opcode89(); void opcode8A(); void opcode8B(); void opcode8C(); void opcode8D(); void opcode8E(); void opcode8F();
        void opcode90(); void opcode91(); void opcode92(); void opcode93(); void opcode94(); void opcode95(); void opcode96(); void opcode97(); void opcode98(); void opcode99(); void opcode9A(); void opcode9B(); void opcode9C(); void opcode9D(); void opcode9E(); void opcode9F();
        void opcodeA0(); void opcodeA1(); void opcodeA2(); void opcodeA3(); void opcodeA4(); void opcodeA5(); void opcodeA6(); void opcodeA7(); void opcodeA8(); void opcodeA9(); void opcodeAA(); void opcodeAB(); void opcodeAC(); void opcodeAD(); void opcodeAE(); void opcodeAF();
        void opcodeB0(); void opcodeB1(); void opcodeB2(); void opcodeB3(); void opcodeB4(); void opcodeB5(); void opcodeB6(); void opcodeB7(); void opcodeB8(); void opcodeB9(); void opcodeBA(); void opcodeBB(); void opcodeBC(); void opcodeBD(); void opcodeBE(); void opcodeBF();
        void opcodeC0(); void opcodeC1(); void opcodeC2(); void opcodeC3(); void opcodeC4(); void opcodeC5(); void opcodeC6(); void opcodeC7(); void opcodeC8(); void opcodeC9(); void opcodeCA(); void opcodeCB(); void opcodeCC(); void opcodeCD(); void opcodeCE(); void opcodeCF();
        void opcodeD0(); void opcodeD1(); void opcodeD2(); void opcodeD3(); void opcodeD4(); void opcodeD5(); void opcodeD6(); void opcodeD7(); void opcodeD8(); void opcodeD9(); void opcodeDA(); void opcodeDB(); void opcodeDC(); void opcodeDD(); void opcodeDE(); void opcodeDF();
        void opcodeE0(); void opcodeE1(); void opcodeE2(); void opcodeE3(); void opcodeE4(); void opcodeE5(); void opcodeE6(); void opcodeE7(); void opcodeE8(); void opcodeE9(); void opcodeEA(); void opcodeEB(); void opcodeEC(); void opcodeED(); void opcodeEE(); void opcodeEF();
        void opcodeF0(); void opcodeF1(); void opcodeF2(); void opcodeF3(); void opcodeF4(); void opcodeF5(); void opcodeF6(); void opcodeF7(); void opcodeF8(); void opcodeF9(); void opcodeFA(); void opcodeFB(); void opcodeFC(); void opcodeFD(); void opcodeFE(); void opcodeFF();

        // CB Opcodes
        void opcodeCB00(); void opcodeCB01(); void opcodeCB02(); void opcodeCB03(); void opcodeCB04(); void opcodeCB05(); void opcodeCB06(); void opcodeCB07(); void opcodeCB08(); void opcodeCB09(); void opcodeCB0A(); void opcodeCB0B(); void opcodeCB0C(); void opcodeCB0D(); void opcodeCB0E(); void opcodeCB0F();
        void opcodeCB10(); void opcodeCB11(); void opcodeCB12(); void opcodeCB13(); void opcodeCB14(); void opcodeCB15(); void opcodeCB16(); void opcodeCB17(); void opcodeCB18(); void opcodeCB19(); void opcodeCB1A(); void opcodeCB1B(); void opcodeCB1C(); void opcodeCB1D(); void opcodeCB1E(); void opcodeCB1F();
        void opcodeCB20(); void opcodeCB21(); void opcodeCB22(); void opcodeCB23(); void opcodeCB24(); void opcodeCB25(); void opcodeCB26(); void opcodeCB27(); void opcodeCB28(); void opcodeCB29(); void opcodeCB2A(); void opcodeCB2B(); void opcodeCB2C(); void opcodeCB2D(); void opcodeCB2E(); void opcodeCB2F();
        void opcodeCB30(); void opcodeCB31(); void opcodeCB32(); void opcodeCB33(); void opcodeCB34(); void opcodeCB35(); void opcodeCB36(); void opcodeCB37(); void opcodeCB38(); void opcodeCB39(); void opcodeCB3A(); void opcodeCB3B(); void opcodeCB3C(); void opcodeCB3D(); void opcodeCB3E(); void opcodeCB3F();
        void opcodeCB40(); void opcodeCB41(); void opcodeCB42(); void opcodeCB43(); void opcodeCB44(); void opcodeCB45(); void opcodeCB46(); void opcodeCB47(); void opcodeCB48(); void opcodeCB49(); void opcodeCB4A(); void opcodeCB4B(); void opcodeCB4C(); void opcodeCB4D(); void opcodeCB4E(); void opcodeCB4F();
        void opcodeCB50(); void opcodeCB51(); void opcodeCB52(); void opcodeCB53(); void opcodeCB54(); void opcodeCB55(); void opcodeCB56(); void opcodeCB57(); void opcodeCB58(); void opcodeCB59(); void opcodeCB5A(); void opcodeCB5B(); void opcodeCB5C(); void opcodeCB5D(); void opcodeCB5E(); void opcodeCB5F();
        void opcodeCB60(); void opcodeCB61(); void opcodeCB62(); void opcodeCB63(); void opcodeCB64(); void opcodeCB65(); void opcodeCB66(); void opcodeCB67(); void opcodeCB68(); void opcodeCB69(); void opcodeCB6A(); void opcodeCB6B(); void opcodeCB6C(); void opcodeCB6D(); void opcodeCB6E(); void opcodeCB6F();
        void opcodeCB70(); void opcodeCB71(); void opcodeCB72(); void opcodeCB73(); void opcodeCB74(); void opcodeCB75(); void opcodeCB76(); void opcodeCB77(); void opcodeCB78(); void opcodeCB79(); void opcodeCB7A(); void opcodeCB7B(); void opcodeCB7C(); void opcodeCB7D(); void opcodeCB7E(); void opcodeCB7F();
        void opcodeCB80(); void opcodeCB81(); void opcodeCB82(); void opcodeCB83(); void opcodeCB84(); void opcodeCB85(); void opcodeCB86(); void opcodeCB87(); void opcodeCB88(); void opcodeCB89(); void opcodeCB8A(); void opcodeCB8B(); void opcodeCB8C(); void opcodeCB8D(); void opcodeCB8E(); void opcodeCB8F();
        void opcodeCB90(); void opcodeCB91(); void opcodeCB92(); void opcodeCB93(); void opcodeCB94(); void opcodeCB95(); void opcodeCB96(); void opcodeCB97(); void opcodeCB98(); void opcodeCB99(); void opcodeCB9A(); void opcodeCB9B(); void opcodeCB9C(); void opcodeCB9D(); void opcodeCB9E(); void opcodeCB9F();
        void opcodeCBA0(); void opcodeCBA1(); void opcodeCBA2(); void opcodeCBA3(); void opcodeCBA4(); void opcodeCBA5(); void opcodeCBA6(); void opcodeCBA7(); void opcodeCBA8(); void opcodeCBA9(); void opcodeCBAA(); void opcodeCBAB(); void opcodeCBAC(); void opcodeCBAD(); void opcodeCBAE(); void opcodeCBAF();
        void opcodeCBB0(); void opcodeCBB1(); void opcodeCBB2(); void opcodeCBB3(); void opcodeCBB4(); void opcodeCBB5(); void opcodeCBB6(); void opcodeCBB7(); void opcodeCBB8(); void opcodeCBB9(); void opcodeCBBA(); void opcodeCBBB(); void opcodeCBBC(); void opcodeCBBD(); void opcodeCBBE(); void opcodeCBBF();
        void opcodeCBC0(); void opcodeCBC1(); void opcodeCBC2(); void opcodeCBC3(); void opcodeCBC4(); void opcodeCBC5(); void opcodeCBC6(); void opcodeCBC7(); void opcodeCBC8(); void opcodeCBC9(); void opcodeCBCA(); void opcodeCBCB(); void opcodeCBCC(); void opcodeCBCD(); void opcodeCBCE(); void opcodeCBCF();
        void opcodeCBD0(); void opcodeCBD1(); void opcodeCBD2(); void opcodeCBD3(); void opcodeCBD4(); void opcodeCBD5(); void opcodeCBD6(); void opcodeCBD7(); void opcodeCBD8(); void opcodeCBD9(); void opcodeCBDA(); void opcodeCBDB(); void opcodeCBDC(); void opcodeCBDD(); void opcodeCBDE(); void opcodeCBDF();
        void opcodeCBE0(); void opcodeCBE1(); void opcodeCBE2(); void opcodeCBE3(); void opcodeCBE4(); void opcodeCBE5(); void opcodeCBE6(); void opcodeCBE7(); void opcodeCBE8(); void opcodeCBE9(); void opcodeCBEA(); void opcodeCBEB(); void opcodeCBEC(); void opcodeCBED(); void opcodeCBEE(); void opcodeCBEF();
        void opcodeCBF0(); void opcodeCBF1(); void opcodeCBF2(); void opcodeCBF3(); void opcodeCBF4(); void opcodeCBF5(); void opcodeCBF6(); void opcodeCBF7(); void opcodeCBF8(); void opcodeCBF9(); void opcodeCBFA(); void opcodeCBFB(); void opcodeCBFC(); void opcodeCBFD(); void opcodeCBFE(); void opcodeCBFF();
    public:
        CPU(Emulator &emulator);
        void initializeCGBPostBoot();
        Cycles tick();
        Cycles executeOpcode(uint8_t opcode, uint16_t opcodePC);
        Cycles executeNormalOpcode(uint8_t opcode, uint16_t opcodePC);
        Cycles executeCBOpcode(uint8_t opcode, uint16_t opcodePC);

        ByteRegister interruptFlag;
        ByteRegister interruptEnabled;
};
