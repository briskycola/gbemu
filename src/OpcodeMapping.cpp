#include "CPU.hpp"

// Credit: jgilchrist
// https://raw.githubusercontent.com/jgilchrist/gbemu/refs/heads/master/src/cpu/opcode_mapping.cc
//
// Game Boy opcode reference
// https://www.pastraiser.com/cpu/gameboy/gameboy_opcodes.html

/**
 * This section contains functions which map to actual opcodes which are executed
 * by the Gameboy's processor.
 */

void CPU::opcode00() { opcodeNop(); }
void CPU::opcode01() { opcodeLd(bc); }
void CPU::opcode02() { opcodeLd(Address(bc), a); }
void CPU::opcode03() { opcodeInc(bc); }
void CPU::opcode04() { opcodeInc(b); }
void CPU::opcode05() { opcodeDec(b); }
void CPU::opcode06() { opcodeLd(b); }
void CPU::opcode07() { opcodeRlca(); }
void CPU::opcode08() { uint16_t word = getWordFromPC(); opcodeLd(Address(word), sp); }
void CPU::opcode09() { opcodeAddHL(bc); }
void CPU::opcode0A() { opcodeLd(a, Address(bc)); }
void CPU::opcode0B() { opcodeDec(bc); }
void CPU::opcode0C() { opcodeInc(c); }
void CPU::opcode0D() { opcodeDec(c); }
void CPU::opcode0E() { opcodeLd(c); }
void CPU::opcode0F() { opcodeRrca(); }
void CPU::opcode10() { opcodeStop(); }
void CPU::opcode11() { opcodeLd(de); }
void CPU::opcode12() { opcodeLd(Address(de), a); }
void CPU::opcode13() { opcodeInc(de); }
void CPU::opcode14() { opcodeInc(d); }
void CPU::opcode15() { opcodeDec(d); }
void CPU::opcode16() { opcodeLd(d); }
void CPU::opcode17() { opcodeRla(); }
void CPU::opcode18() { opcodeJr(); }
void CPU::opcode19() { opcodeAddHL(de); }
void CPU::opcode1A() { opcodeLd(a, Address(de)); }
void CPU::opcode1B() { opcodeDec(de); }
void CPU::opcode1C() { opcodeInc(e); }
void CPU::opcode1D() { opcodeDec(e); }
void CPU::opcode1E() { opcodeLd(e); }
void CPU::opcode1F() { opcodeRra(); }
void CPU::opcode20() { opcodeJr(Condition::NZ); }
void CPU::opcode21() { opcodeLd(hl); }
void CPU::opcode22() { opcodeLdi(Address(hl), a); }
void CPU::opcode23() { opcodeInc(hl); }
void CPU::opcode24() { opcodeInc(h); }
void CPU::opcode25() { opcodeDec(h); }
void CPU::opcode26() { opcodeLd(h); }
void CPU::opcode27() { opcodeDaa(); }
void CPU::opcode28() { opcodeJr(Condition::Z); }
void CPU::opcode29() { opcodeAddHL(hl); }
void CPU::opcode2A() { opcodeLdi(a, Address(hl)); }
void CPU::opcode2B() { opcodeDec(hl); }
void CPU::opcode2C() { opcodeInc(l); }
void CPU::opcode2D() { opcodeDec(l); }
void CPU::opcode2E() { opcodeLd(l); }
void CPU::opcode2F() { opcodeCpl(); }
void CPU::opcode30() { opcodeJr(Condition::NC); }
void CPU::opcode31() { opcodeLd(sp); }
void CPU::opcode32() { opcodeLdd(Address(hl), a); }
void CPU::opcode33() { opcodeInc(sp); }
void CPU::opcode34() { opcodeInc(Address(hl)); }
void CPU::opcode35() { opcodeDec(Address(hl)); }
void CPU::opcode36() { opcodeLd(Address(hl)); }
void CPU::opcode37() { opcodeScf(); }
void CPU::opcode38() { opcodeJr(Condition::C); }
void CPU::opcode39() { opcodeAddHL(sp); }
void CPU::opcode3A() { opcodeLdd(a, Address(hl)); }
void CPU::opcode3B() { opcodeDec(sp); }
void CPU::opcode3C() { opcodeInc(a); }
void CPU::opcode3D() { opcodeDec(a); }
void CPU::opcode3E() { opcodeLd(a); }
void CPU::opcode3F() { opcodeCcf(); }
void CPU::opcode40() { opcodeLd(b, b); }
void CPU::opcode41() { opcodeLd(b, c); }
void CPU::opcode42() { opcodeLd(b, d); }
void CPU::opcode43() { opcodeLd(b, e); }
void CPU::opcode44() { opcodeLd(b, h); }
void CPU::opcode45() { opcodeLd(b, l); }
void CPU::opcode46() { opcodeLd(b, Address(hl)); }
void CPU::opcode47() { opcodeLd(b, a); }
void CPU::opcode48() { opcodeLd(c, b); }
void CPU::opcode49() { opcodeLd(c, c); }
void CPU::opcode4A() { opcodeLd(c, d); }
void CPU::opcode4B() { opcodeLd(c, e); }
void CPU::opcode4C() { opcodeLd(c, h); }
void CPU::opcode4D() { opcodeLd(c, l); }
void CPU::opcode4E() { opcodeLd(c, Address(hl)); }
void CPU::opcode4F() { opcodeLd(c, a); }
void CPU::opcode50() { opcodeLd(d, b); }
void CPU::opcode51() { opcodeLd(d, c); }
void CPU::opcode52() { opcodeLd(d, d); }
void CPU::opcode53() { opcodeLd(d, e); }
void CPU::opcode54() { opcodeLd(d, h); }
void CPU::opcode55() { opcodeLd(d, l); }
void CPU::opcode56() { opcodeLd(d, Address(hl)); }
void CPU::opcode57() { opcodeLd(d, a); }
void CPU::opcode58() { opcodeLd(e, b); }
void CPU::opcode59() { opcodeLd(e, c); }
void CPU::opcode5A() { opcodeLd(e, d); }
void CPU::opcode5B() { opcodeLd(e, e); }
void CPU::opcode5C() { opcodeLd(e, h); }
void CPU::opcode5D() { opcodeLd(e, l); }
void CPU::opcode5E() { opcodeLd(e, Address(hl)); }
void CPU::opcode5F() { opcodeLd(e, a); }
void CPU::opcode60() { opcodeLd(h, b); }
void CPU::opcode61() { opcodeLd(h, c); }
void CPU::opcode62() { opcodeLd(h, d); }
void CPU::opcode63() { opcodeLd(h, e); }
void CPU::opcode64() { opcodeLd(h, h); }
void CPU::opcode65() { opcodeLd(h, l); }
void CPU::opcode66() { opcodeLd(h, Address(hl)); }
void CPU::opcode67() { opcodeLd(h, a); }
void CPU::opcode68() { opcodeLd(l, b); }
void CPU::opcode69() { opcodeLd(l, c); }
void CPU::opcode6A() { opcodeLd(l, d); }
void CPU::opcode6B() { opcodeLd(l, e); }
void CPU::opcode6C() { opcodeLd(l, h); }
void CPU::opcode6D() { opcodeLd(l, l); }
void CPU::opcode6E() { opcodeLd(l, Address(hl)); }
void CPU::opcode6F() { opcodeLd(l, a); }
void CPU::opcode70() { opcodeLd(Address(hl), b); }
void CPU::opcode71() { opcodeLd(Address(hl), c); }
void CPU::opcode72() { opcodeLd(Address(hl), d); }
void CPU::opcode73() { opcodeLd(Address(hl), e); }
void CPU::opcode74() { opcodeLd(Address(hl), h); }
void CPU::opcode75() { opcodeLd(Address(hl), l); }
void CPU::opcode76() { opcodeHalt(); }
void CPU::opcode77() { opcodeLd(Address(hl), a); }
void CPU::opcode78() { opcodeLd(a, b); }
void CPU::opcode79() { opcodeLd(a, c); }
void CPU::opcode7A() { opcodeLd(a, d); }
void CPU::opcode7B() { opcodeLd(a, e); }
void CPU::opcode7C() { opcodeLd(a, h); }
void CPU::opcode7D() { opcodeLd(a, l); }
void CPU::opcode7E() { opcodeLd(a, Address(hl)); }
void CPU::opcode7F() { opcodeLd(a, a); }
void CPU::opcode80() { opcodeAddA(b); }
void CPU::opcode81() { opcodeAddA(c); }
void CPU::opcode82() { opcodeAddA(d); }
void CPU::opcode83() { opcodeAddA(e); }
void CPU::opcode84() { opcodeAddA(h); }
void CPU::opcode85() { opcodeAddA(l); }
void CPU::opcode86() { opcodeAddA(Address(hl)); }
void CPU::opcode87() { opcodeAddA(a); }
void CPU::opcode88() { opcodeAdc(b); }
void CPU::opcode89() { opcodeAdc(c); }
void CPU::opcode8A() { opcodeAdc(d); }
void CPU::opcode8B() { opcodeAdc(e); }
void CPU::opcode8C() { opcodeAdc(h); }
void CPU::opcode8D() { opcodeAdc(l); }
void CPU::opcode8E() { opcodeAdc(Address(hl)); }
void CPU::opcode8F() { opcodeAdc(a); }
void CPU::opcode90() { opcodeSub(b); }
void CPU::opcode91() { opcodeSub(c); }
void CPU::opcode92() { opcodeSub(d); }
void CPU::opcode93() { opcodeSub(e); }
void CPU::opcode94() { opcodeSub(h); }
void CPU::opcode95() { opcodeSub(l); }
void CPU::opcode96() { opcodeSub(Address(hl)); }
void CPU::opcode97() { opcodeSub(a); }
void CPU::opcode98() { opcodeSbc(b); }
void CPU::opcode99() { opcodeSbc(c); }
void CPU::opcode9A() { opcodeSbc(d); }
void CPU::opcode9B() { opcodeSbc(e); }
void CPU::opcode9C() { opcodeSbc(h); }
void CPU::opcode9D() { opcodeSbc(l); }
void CPU::opcode9E() { opcodeSbc(Address(hl)); }
void CPU::opcode9F() { opcodeSbc(a); }
void CPU::opcodeA0() { opcodeAnd(b); }
void CPU::opcodeA1() { opcodeAnd(c); }
void CPU::opcodeA2() { opcodeAnd(d); }
void CPU::opcodeA3() { opcodeAnd(e); }
void CPU::opcodeA4() { opcodeAnd(h); }
void CPU::opcodeA5() { opcodeAnd(l); }
void CPU::opcodeA6() { opcodeAnd(Address(hl)); }
void CPU::opcodeA7() { opcodeAnd(a); }
void CPU::opcodeA8() { opcodeXor(b); }
void CPU::opcodeA9() { opcodeXor(c); }
void CPU::opcodeAA() { opcodeXor(d); }
void CPU::opcodeAB() { opcodeXor(e); }
void CPU::opcodeAC() { opcodeXor(h); }
void CPU::opcodeAD() { opcodeXor(l); }
void CPU::opcodeAE() { opcodeXor(Address(hl)); }
void CPU::opcodeAF() { opcodeXor(a); }
void CPU::opcodeB0() { opcodeOr(b); }
void CPU::opcodeB1() { opcodeOr(c); }
void CPU::opcodeB2() { opcodeOr(d); }
void CPU::opcodeB3() { opcodeOr(e); }
void CPU::opcodeB4() { opcodeOr(h); }
void CPU::opcodeB5() { opcodeOr(l); }
void CPU::opcodeB6() { opcodeOr(Address(hl)); }
void CPU::opcodeB7() { opcodeOr(a); }
void CPU::opcodeB8() { opcodeCp(b); }
void CPU::opcodeB9() { opcodeCp(c); }
void CPU::opcodeBA() { opcodeCp(d); }
void CPU::opcodeBB() { opcodeCp(e); }
void CPU::opcodeBC() { opcodeCp(h); }
void CPU::opcodeBD() { opcodeCp(l); }
void CPU::opcodeBE() { opcodeCp(Address(hl)); }
void CPU::opcodeBF() { opcodeCp(a); }
void CPU::opcodeC0() { opcodeRet(Condition::NZ); }
void CPU::opcodeC1() { opcodePop(bc); }
void CPU::opcodeC2() { opcodeJp(Condition::NZ); }
void CPU::opcodeC3() { opcodeJp(); }
void CPU::opcodeC4() { opcodeCall(Condition::NZ); }
void CPU::opcodeC5() { opcodePush(bc); }
void CPU::opcodeC6() { opcodeAddA(); }
void CPU::opcodeC7() { opcodeRst(RST1); }
void CPU::opcodeC8() { opcodeRet(Condition::Z); }
void CPU::opcodeC9() { opcodeRet(); }
void CPU::opcodeCA() { opcodeJp(Condition::Z); }
void CPU::opcodeCB() { /* External Ops */ }
void CPU::opcodeCC() { opcodeCall(Condition::Z); }
void CPU::opcodeCD() { opcodeCall(); }
void CPU::opcodeCE() { opcodeAdc(); }
void CPU::opcodeCF() { opcodeRst(RST2); }
void CPU::opcodeD0() { opcodeRet(Condition::NC); }
void CPU::opcodeD1() { opcodePop(de); }
void CPU::opcodeD2() { opcodeJp(Condition::NC); }
void CPU::opcodeD3() { /* Undefined */ }
void CPU::opcodeD4() { opcodeCall(Condition::NC); }
void CPU::opcodeD5() { opcodePush(de); }
void CPU::opcodeD6() { opcodeSub(); }
void CPU::opcodeD7() { opcodeRst(RST3); }
void CPU::opcodeD8() { opcodeRet(Condition::C); }
void CPU::opcodeD9() { opcodeReti(); }
void CPU::opcodeDA() { opcodeJp(Condition::C); }
void CPU::opcodeDB() { /* Undefined */ }
void CPU::opcodeDC() { opcodeCall(Condition::C); }
void CPU::opcodeDD() { /* Undefined */ }
void CPU::opcodeDE() { opcodeSbc(); }
void CPU::opcodeDF() { opcodeRst(RST4); }
void CPU::opcodeE0() { opcodeLdhIntoData(); }
void CPU::opcodeE1() { opcodePop(hl); }
void CPU::opcodeE2() { opcodeLdhIntoC(); }
void CPU::opcodeE3() { /* Undefined */ }
void CPU::opcodeE4() { /* Undefined */ }
void CPU::opcodeE5() { opcodePush(hl); }
void CPU::opcodeE6() { opcodeAnd(); }
void CPU::opcodeE7() { opcodeRst(RST5); }
void CPU::opcodeE8() { opcodeAddSP(); }
void CPU::opcodeE9() { opcodeJp(Address(hl)); }
void CPU::opcodeEA() { opcodeLdToAddr(a); }
void CPU::opcodeEB() { /* Undefined */ }
void CPU::opcodeEC() { /* Undefined */ }
void CPU::opcodeED() { /* Undefined */ }
void CPU::opcodeEE() { opcodeXor(); }
void CPU::opcodeEF() { opcodeRst(RST6); }
void CPU::opcodeF0() { opcodeLdhIntoA(); }
void CPU::opcodeF1() { opcodePop(af); }
void CPU::opcodeF2() { opcodeLdhCIntoA(); }
void CPU::opcodeF3() { opcodeDi(); }
void CPU::opcodeF4() { /* Undefined */ }
void CPU::opcodeF5() { opcodePush(af); }
void CPU::opcodeF6() { opcodeOr(); }
void CPU::opcodeF7() { opcodeRst(RST7); }
void CPU::opcodeF8() { opcodeLdhl(); }
void CPU::opcodeF9() { opcodeLd(sp, hl); }
void CPU::opcodeFA() { opcodeLdFromAddr(a); }
void CPU::opcodeFB() { opcodeEi(); }
void CPU::opcodeFC() { /* Undefined */ }
void CPU::opcodeFD() { /* Undefined */ }
void CPU::opcodeFE() { opcodeCp(); }
void CPU::opcodeFF() { opcodeRst(RST8); }

/**
 * This section contains two-byte opcodes, which are triggered when prefixed with
 * the CB instruction above.
 */

void CPU::opcodeCB00() { opcodeRlc(b); }
void CPU::opcodeCB01() { opcodeRlc(c); }
void CPU::opcodeCB02() { opcodeRlc(d); }
void CPU::opcodeCB03() { opcodeRlc(e); }
void CPU::opcodeCB04() { opcodeRlc(h); }
void CPU::opcodeCB05() { opcodeRlc(l); }
void CPU::opcodeCB06() { opcodeRlc(Address(hl)); }
void CPU::opcodeCB07() { opcodeRlc(a); }
void CPU::opcodeCB08() { opcodeRrc(b); }
void CPU::opcodeCB09() { opcodeRrc(c); }
void CPU::opcodeCB0A() { opcodeRrc(d); }
void CPU::opcodeCB0B() { opcodeRrc(e); }
void CPU::opcodeCB0C() { opcodeRrc(h); }
void CPU::opcodeCB0D() { opcodeRrc(l); }
void CPU::opcodeCB0E() { opcodeRrc(Address(hl)); }
void CPU::opcodeCB0F() { opcodeRrc(a); }
void CPU::opcodeCB10() { opcodeRl(b); }
void CPU::opcodeCB11() { opcodeRl(c); }
void CPU::opcodeCB12() { opcodeRl(d); }
void CPU::opcodeCB13() { opcodeRl(e); }
void CPU::opcodeCB14() { opcodeRl(h); }
void CPU::opcodeCB15() { opcodeRl(l); }
void CPU::opcodeCB16() { opcodeRl(Address(hl)); }
void CPU::opcodeCB17() { opcodeRl(a); }
void CPU::opcodeCB18() { opcodeRr(b); }
void CPU::opcodeCB19() { opcodeRr(c); }
void CPU::opcodeCB1A() { opcodeRr(d); }
void CPU::opcodeCB1B() { opcodeRr(e); }
void CPU::opcodeCB1C() { opcodeRr(h); }
void CPU::opcodeCB1D() { opcodeRr(l); }
void CPU::opcodeCB1E() { opcodeRr(Address(hl)); }
void CPU::opcodeCB1F() { opcodeRr(a); }
void CPU::opcodeCB20() { opcodeSla(b); }
void CPU::opcodeCB21() { opcodeSla(c); }
void CPU::opcodeCB22() { opcodeSla(d); }
void CPU::opcodeCB23() { opcodeSla(e); }
void CPU::opcodeCB24() { opcodeSla(h); }
void CPU::opcodeCB25() { opcodeSla(l); }
void CPU::opcodeCB26() { opcodeSla(Address(hl)); }
void CPU::opcodeCB27() { opcodeSla(a); }
void CPU::opcodeCB28() { opcodeSra(b); }
void CPU::opcodeCB29() { opcodeSra(c); }
void CPU::opcodeCB2A() { opcodeSra(d); }
void CPU::opcodeCB2B() { opcodeSra(e); }
void CPU::opcodeCB2C() { opcodeSra(h); }
void CPU::opcodeCB2D() { opcodeSra(l); }
void CPU::opcodeCB2E() { opcodeSra(Address(hl)); }
void CPU::opcodeCB2F() { opcodeSra(a); }
void CPU::opcodeCB30() { opcodeSwap(b); }
void CPU::opcodeCB31() { opcodeSwap(c); }
void CPU::opcodeCB32() { opcodeSwap(d); }
void CPU::opcodeCB33() { opcodeSwap(e); }
void CPU::opcodeCB34() { opcodeSwap(h); }
void CPU::opcodeCB35() { opcodeSwap(l); }
void CPU::opcodeCB36() { opcodeSwap(Address(hl)); }
void CPU::opcodeCB37() { opcodeSwap(a); }
void CPU::opcodeCB38() { opcodeSrl(b); }
void CPU::opcodeCB39() { opcodeSrl(c); }
void CPU::opcodeCB3A() { opcodeSrl(d); }
void CPU::opcodeCB3B() { opcodeSrl(e); }
void CPU::opcodeCB3C() { opcodeSrl(h); }
void CPU::opcodeCB3D() { opcodeSrl(l); }
void CPU::opcodeCB3E() { opcodeSrl(Address(hl)); }
void CPU::opcodeCB3F() { opcodeSrl(a); }
void CPU::opcodeCB40() { opcodeBit(0, b); }
void CPU::opcodeCB41() { opcodeBit(0, c); }
void CPU::opcodeCB42() { opcodeBit(0, d); }
void CPU::opcodeCB43() { opcodeBit(0, e); }
void CPU::opcodeCB44() { opcodeBit(0, h); }
void CPU::opcodeCB45() { opcodeBit(0, l); }
void CPU::opcodeCB46() { opcodeBit(0, Address(hl)); }
void CPU::opcodeCB47() { opcodeBit(0, a); }
void CPU::opcodeCB48() { opcodeBit(1, b); }
void CPU::opcodeCB49() { opcodeBit(1, c); }
void CPU::opcodeCB4A() { opcodeBit(1, d); }
void CPU::opcodeCB4B() { opcodeBit(1, e); }
void CPU::opcodeCB4C() { opcodeBit(1, h); }
void CPU::opcodeCB4D() { opcodeBit(1, l); }
void CPU::opcodeCB4E() { opcodeBit(1, Address(hl)); }
void CPU::opcodeCB4F() { opcodeBit(1, a); }
void CPU::opcodeCB50() { opcodeBit(2, b); }
void CPU::opcodeCB51() { opcodeBit(2, c); }
void CPU::opcodeCB52() { opcodeBit(2, d); }
void CPU::opcodeCB53() { opcodeBit(2, e); }
void CPU::opcodeCB54() { opcodeBit(2, h); }
void CPU::opcodeCB55() { opcodeBit(2, l); }
void CPU::opcodeCB56() { opcodeBit(2, Address(hl)); }
void CPU::opcodeCB57() { opcodeBit(2, a); }
void CPU::opcodeCB58() { opcodeBit(3, b); }
void CPU::opcodeCB59() { opcodeBit(3, c); }
void CPU::opcodeCB5A() { opcodeBit(3, d); }
void CPU::opcodeCB5B() { opcodeBit(3, e); }
void CPU::opcodeCB5C() { opcodeBit(3, h); }
void CPU::opcodeCB5D() { opcodeBit(3, l); }
void CPU::opcodeCB5E() { opcodeBit(3, Address(hl)); }
void CPU::opcodeCB5F() { opcodeBit(3, a); }
void CPU::opcodeCB60() { opcodeBit(4, b); }
void CPU::opcodeCB61() { opcodeBit(4, c); }
void CPU::opcodeCB62() { opcodeBit(4, d); }
void CPU::opcodeCB63() { opcodeBit(4, e); }
void CPU::opcodeCB64() { opcodeBit(4, h); }
void CPU::opcodeCB65() { opcodeBit(4, l); }
void CPU::opcodeCB66() { opcodeBit(4, Address(hl)); }
void CPU::opcodeCB67() { opcodeBit(4, a); }
void CPU::opcodeCB68() { opcodeBit(5, b); }
void CPU::opcodeCB69() { opcodeBit(5, c); }
void CPU::opcodeCB6A() { opcodeBit(5, d); }
void CPU::opcodeCB6B() { opcodeBit(5, e); }
void CPU::opcodeCB6C() { opcodeBit(5, h); }
void CPU::opcodeCB6D() { opcodeBit(5, l); }
void CPU::opcodeCB6E() { opcodeBit(5, Address(hl)); }
void CPU::opcodeCB6F() { opcodeBit(5, a); }
void CPU::opcodeCB70() { opcodeBit(6, b); }
void CPU::opcodeCB71() { opcodeBit(6, c); }
void CPU::opcodeCB72() { opcodeBit(6, d); }
void CPU::opcodeCB73() { opcodeBit(6, e); }
void CPU::opcodeCB74() { opcodeBit(6, h); }
void CPU::opcodeCB75() { opcodeBit(6, l); }
void CPU::opcodeCB76() { opcodeBit(6, Address(hl)); }
void CPU::opcodeCB77() { opcodeBit(6, a); }
void CPU::opcodeCB78() { opcodeBit(7, b); }
void CPU::opcodeCB79() { opcodeBit(7, c); }
void CPU::opcodeCB7A() { opcodeBit(7, d); }
void CPU::opcodeCB7B() { opcodeBit(7, e); }
void CPU::opcodeCB7C() { opcodeBit(7, h); }
void CPU::opcodeCB7D() { opcodeBit(7, l); }
void CPU::opcodeCB7E() { opcodeBit(7, Address(hl)); }
void CPU::opcodeCB7F() { opcodeBit(7, a); }
void CPU::opcodeCB80() { opcodeRes(0, b); }
void CPU::opcodeCB81() { opcodeRes(0, c); }
void CPU::opcodeCB82() { opcodeRes(0, d); }
void CPU::opcodeCB83() { opcodeRes(0, e); }
void CPU::opcodeCB84() { opcodeRes(0, h); }
void CPU::opcodeCB85() { opcodeRes(0, l); }
void CPU::opcodeCB86() { opcodeRes(0, Address(hl)); }
void CPU::opcodeCB87() { opcodeRes(0, a); }
void CPU::opcodeCB88() { opcodeRes(1, b); }
void CPU::opcodeCB89() { opcodeRes(1, c); }
void CPU::opcodeCB8A() { opcodeRes(1, d); }
void CPU::opcodeCB8B() { opcodeRes(1, e); }
void CPU::opcodeCB8C() { opcodeRes(1, h); }
void CPU::opcodeCB8D() { opcodeRes(1, l); }
void CPU::opcodeCB8E() { opcodeRes(1, Address(hl)); }
void CPU::opcodeCB8F() { opcodeRes(1, a); }
void CPU::opcodeCB90() { opcodeRes(2, b); }
void CPU::opcodeCB91() { opcodeRes(2, c); }
void CPU::opcodeCB92() { opcodeRes(2, d); }
void CPU::opcodeCB93() { opcodeRes(2, e); }
void CPU::opcodeCB94() { opcodeRes(2, h); }
void CPU::opcodeCB95() { opcodeRes(2, l); }
void CPU::opcodeCB96() { opcodeRes(2, Address(hl)); }
void CPU::opcodeCB97() { opcodeRes(2, a); }
void CPU::opcodeCB98() { opcodeRes(3, b); }
void CPU::opcodeCB99() { opcodeRes(3, c); }
void CPU::opcodeCB9A() { opcodeRes(3, d); }
void CPU::opcodeCB9B() { opcodeRes(3, e); }
void CPU::opcodeCB9C() { opcodeRes(3, h); }
void CPU::opcodeCB9D() { opcodeRes(3, l); }
void CPU::opcodeCB9E() { opcodeRes(3, Address(hl)); }
void CPU::opcodeCB9F() { opcodeRes(3, a); }
void CPU::opcodeCBA0() { opcodeRes(4, b); }
void CPU::opcodeCBA1() { opcodeRes(4, c); }
void CPU::opcodeCBA2() { opcodeRes(4, d); }
void CPU::opcodeCBA3() { opcodeRes(4, e); }
void CPU::opcodeCBA4() { opcodeRes(4, h); }
void CPU::opcodeCBA5() { opcodeRes(4, l); }
void CPU::opcodeCBA6() { opcodeRes(4, Address(hl)); }
void CPU::opcodeCBA7() { opcodeRes(4, a); }
void CPU::opcodeCBA8() { opcodeRes(5, b); }
void CPU::opcodeCBA9() { opcodeRes(5, c); }
void CPU::opcodeCBAA() { opcodeRes(5, d); }
void CPU::opcodeCBAB() { opcodeRes(5, e); }
void CPU::opcodeCBAC() { opcodeRes(5, h); }
void CPU::opcodeCBAD() { opcodeRes(5, l); }
void CPU::opcodeCBAE() { opcodeRes(5, Address(hl)); }
void CPU::opcodeCBAF() { opcodeRes(5, a); }
void CPU::opcodeCBB0() { opcodeRes(6, b); }
void CPU::opcodeCBB1() { opcodeRes(6, c); }
void CPU::opcodeCBB2() { opcodeRes(6, d); }
void CPU::opcodeCBB3() { opcodeRes(6, e); }
void CPU::opcodeCBB4() { opcodeRes(6, h); }
void CPU::opcodeCBB5() { opcodeRes(6, l); }
void CPU::opcodeCBB6() { opcodeRes(6, Address(hl)); }
void CPU::opcodeCBB7() { opcodeRes(6, a); }
void CPU::opcodeCBB8() { opcodeRes(7, b); }
void CPU::opcodeCBB9() { opcodeRes(7, c); }
void CPU::opcodeCBBA() { opcodeRes(7, d); }
void CPU::opcodeCBBB() { opcodeRes(7, e); }
void CPU::opcodeCBBC() { opcodeRes(7, h); }
void CPU::opcodeCBBD() { opcodeRes(7, l); }
void CPU::opcodeCBBE() { opcodeRes(7, Address(hl)); }
void CPU::opcodeCBBF() { opcodeRes(7, a); }
void CPU::opcodeCBC0() { opcodeSet(0, b); }
void CPU::opcodeCBC1() { opcodeSet(0, c); }
void CPU::opcodeCBC2() { opcodeSet(0, d); }
void CPU::opcodeCBC3() { opcodeSet(0, e); }
void CPU::opcodeCBC4() { opcodeSet(0, h); }
void CPU::opcodeCBC5() { opcodeSet(0, l); }
void CPU::opcodeCBC6() { opcodeSet(0, Address(hl)); }
void CPU::opcodeCBC7() { opcodeSet(0, a); }
void CPU::opcodeCBC8() { opcodeSet(1, b); }
void CPU::opcodeCBC9() { opcodeSet(1, c); }
void CPU::opcodeCBCA() { opcodeSet(1, d); }
void CPU::opcodeCBCB() { opcodeSet(1, e); }
void CPU::opcodeCBCC() { opcodeSet(1, h); }
void CPU::opcodeCBCD() { opcodeSet(1, l); }
void CPU::opcodeCBCE() { opcodeSet(1, Address(hl)); }
void CPU::opcodeCBCF() { opcodeSet(1, a); }
void CPU::opcodeCBD0() { opcodeSet(2, b); }
void CPU::opcodeCBD1() { opcodeSet(2, c); }
void CPU::opcodeCBD2() { opcodeSet(2, d); }
void CPU::opcodeCBD3() { opcodeSet(2, e); }
void CPU::opcodeCBD4() { opcodeSet(2, h); }
void CPU::opcodeCBD5() { opcodeSet(2, l); }
void CPU::opcodeCBD6() { opcodeSet(2, Address(hl)); }
void CPU::opcodeCBD7() { opcodeSet(2, a); }
void CPU::opcodeCBD8() { opcodeSet(3, b); }
void CPU::opcodeCBD9() { opcodeSet(3, c); }
void CPU::opcodeCBDA() { opcodeSet(3, d); }
void CPU::opcodeCBDB() { opcodeSet(3, e); }
void CPU::opcodeCBDC() { opcodeSet(3, h); }
void CPU::opcodeCBDD() { opcodeSet(3, l); }
void CPU::opcodeCBDE() { opcodeSet(3, Address(hl)); }
void CPU::opcodeCBDF() { opcodeSet(3, a); }
void CPU::opcodeCBE0() { opcodeSet(4, b); }
void CPU::opcodeCBE1() { opcodeSet(4, c); }
void CPU::opcodeCBE2() { opcodeSet(4, d); }
void CPU::opcodeCBE3() { opcodeSet(4, e); }
void CPU::opcodeCBE4() { opcodeSet(4, h); }
void CPU::opcodeCBE5() { opcodeSet(4, l); }
void CPU::opcodeCBE6() { opcodeSet(4, Address(hl)); }
void CPU::opcodeCBE7() { opcodeSet(4, a); }
void CPU::opcodeCBE8() { opcodeSet(5, b); }
void CPU::opcodeCBE9() { opcodeSet(5, c); }
void CPU::opcodeCBEA() { opcodeSet(5, d); }
void CPU::opcodeCBEB() { opcodeSet(5, e); }
void CPU::opcodeCBEC() { opcodeSet(5, h); }
void CPU::opcodeCBED() { opcodeSet(5, l); }
void CPU::opcodeCBEE() { opcodeSet(5, Address(hl)); }
void CPU::opcodeCBEF() { opcodeSet(5, a); }
void CPU::opcodeCBF0() { opcodeSet(6, b); }
void CPU::opcodeCBF1() { opcodeSet(6, c); }
void CPU::opcodeCBF2() { opcodeSet(6, d); }
void CPU::opcodeCBF3() { opcodeSet(6, e); }
void CPU::opcodeCBF4() { opcodeSet(6, h); }
void CPU::opcodeCBF5() { opcodeSet(6, l); }
void CPU::opcodeCBF6() { opcodeSet(6, Address(hl)); }
void CPU::opcodeCBF7() { opcodeSet(6, a); }
void CPU::opcodeCBF8() { opcodeSet(7, b); }
void CPU::opcodeCBF9() { opcodeSet(7, c); }
void CPU::opcodeCBFA() { opcodeSet(7, d); }
void CPU::opcodeCBFB() { opcodeSet(7, e); }
void CPU::opcodeCBFC() { opcodeSet(7, h); }
void CPU::opcodeCBFD() { opcodeSet(7, l); }
void CPU::opcodeCBFE() { opcodeSet(7, Address(hl)); }
void CPU::opcodeCBFF() { opcodeSet(7, a); }
