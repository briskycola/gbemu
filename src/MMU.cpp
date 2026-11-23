#include "MMU.hpp"
#include "Address.hpp"
#include "Emulator.hpp"
#include "CPU.hpp"
#include "Timer.hpp"
#include "Serial.hpp"
#include "Input.hpp"
#include "IORegisters.hpp"
#include "Common.hpp"
#include <algorithm>
#include <iostream>


MMU::MMU(Emulator &emulator) :
    emulator(emulator)
{
    workRAM = std::vector<uint8_t>(mmu::WRAM_SIZE_BYTES);
    oamRAM = std::vector<uint8_t>(mmu::OAM_SIZE_BYTES);
    highRAM = std::vector<uint8_t>(mmu::HRAM_SIZE_BYTES);
}

uint8_t MMU::read(const Address &address, bool bypassPPURestrictions) const
{
    // While OAM DMA runs the CPU cannot reach OAM, and reads from the
    // bus the DMA is using return the byte currently being transferred
    if (!bypassPPURestrictions && oamDMABlocksCPU(address.getValue()))
        return address.getValue() >= mmu::OAM_ADDRESS_START ? io::UNMAPPED_READ_VALUE : oamDMALastByte;

    // Cartridge ROM
    if (address.inRange(mmu::CARTRIDGE_ROM_ADDRESS_START, mmu::CARTRIDGE_ROM_ADDRESS_END))
    {
        // Until it is unmapped, the boot ROM covers 0x0000-0x00FF and, for the
        // larger Game Boy Color one, 0x0200-0x08FF. The cartridge header at
        // 0x0100-0x01FF stays visible either way.
        const uint16_t value = address.getValue();
        const bool inBootROM = value < mmu::DMG_BOOT_ROM_SIZE ||
            (value >= mmu::CGB_BOOT_ROM_SECOND_PART_START && value < bootROM.size());
        if (inBootROM && isBootROMActive()) return bootROM[value];
        return emulator.cartridge->read(address);
    }

    // VRAM
    if (address.inRange(mmu::VRAM_ADDRESS_START, mmu::VRAM_ADDRESS_END))
    {
        if (!bypassPPURestrictions && !emulator.ppu.cpuCanAccessVRAM()) return io::UNMAPPED_READ_VALUE;
        return emulator.ppu.read(address.getValue() - mmu::VRAM_ADDRESS_START);
    }

    // Cartridge RAM (external)
    if (address.inRange(mmu::CARTRIDGE_RAM_ADDRESS_START, mmu::CARTRIDGE_RAM_ADDRESS_END))
    {
        return emulator.cartridge->read(address);
    }

    // WRAM
    if (address.inRange(mmu::WRAM_ADDRESS_START, mmu::WRAM_ADDRESS_END))
    {
        const uint16_t offset = address.getValue() - mmu::WRAM_OFFSET;

        // The second 4 KB of work RAM is switchable on the CGB
        if (emulator.cgbMode && offset >= mmu::WRAM_BANK_SIZE)
            return workRAM.at(static_cast<size_t>(workRAMBank) * mmu::WRAM_BANK_SIZE + (offset - mmu::WRAM_BANK_SIZE));
        return workRAM.at(offset);
    }

    // Echo RAM
    if (address.inRange(mmu::ECHO_RAM_ADDRESS_START, mmu::ECHO_RAM_ADDRESS_END))
    {
        return read(address.getValue() - mmu::ECHO_RAM_OFFSET, bypassPPURestrictions);
    }

    // OAM
    if (address.inRange(mmu::OAM_ADDRESS_START, mmu::OAM_ADDRESS_END))
    {
        if (!bypassPPURestrictions && !emulator.ppu.cpuCanAccessOAM()) return io::UNMAPPED_READ_VALUE;
        return oamRAM.at(address.getValue() - mmu::OAM_OFFSET);
    }

    // Reading from unsuable memory
    if (address.inRange(mmu::UNUSABLE_ADDRESS_START, mmu::UNUSABLE_ADDRESS_END))
    {
        std::cerr << "Reading from unusable memory: " << std::hex << address.getValue() << "\n";
        return io::UNMAPPED_READ_VALUE;
    }

    // I/O Registers
    if (address.inRange(mmu::IO_ADDRESS_START, mmu::IO_ADDRESS_END))
    {
        return readIO(address);
    }

    // HRAM
    if (address.inRange(mmu::HRAM_ADDRESS_START, mmu::HRAM_ADDRESS_END))
    {
        return highRAM.at(address.getValue() - mmu::HRAM_OFFSET);
    }

    // Interrupt Enable Register
    if (address == mmu::INTERRUPT_ENABLE_REGISTER)
    {
        // The unused upper bits read as 1
        return static_cast<uint8_t>(~cpu::INTERRUPT_MASK | (emulator.cpu.interruptEnabled.getValue() & cpu::INTERRUPT_MASK));
    }
    return io::UNMAPPED_READ_VALUE;
}

uint8_t MMU::readIO(const Address &address) const
{
    // Audio registers and wave RAM, including the
    // unused addresses between them
    if (address.inRange(io::AUDIO_ADDRESS_START, io::AUDIO_ADDRESS_END))
    {
        return emulator.apu.read(address.getValue());
    }

    switch (address.getValue())
    {
        case io::JOYPAD_REGISTER:
        {
            uint8_t input = emulator.input.getInput();

            // With neither half of the joypad selected, the
            // Super Game Boy reports the current player instead
            if (emulator.sgbMode && (input & input::JOYPAD_SELECT_MASK) == input::JOYPAD_SELECT_MASK)
                input = static_cast<uint8_t>((input & ~input::JOYPAD_KEYS_MASK) | emulator.sgb.getPlayerId());
            return input;
        }
        case io::SERIAL_DATA_REGISTER: return emulator.serial.read();
        case io::SERIAL_CONTROL_REGISTER: return io::UNMAPPED_READ_VALUE;

        // Timer. Only the low 3 bits of TAC are used;
        // the rest read as 1.
        case io::DIVIDER_REGISTER: return emulator.timer.getDivider();
        case io::TIMER_COUNTER_REGISTER: return emulator.timer.getTimer();
        case io::TIMER_MODULO_REGISTER: return emulator.timer.getTimerModulo();
        case io::TIMER_CONTROL_REGISTER: return 0xF8 | emulator.timer.getTimerControl();

        // The unused upper bits of IF read as 1
        case io::INTERRUPT_FLAG_REGISTER:
            return static_cast<uint8_t>(~cpu::INTERRUPT_MASK | (emulator.cpu.interruptFlag.getValue() & cpu::INTERRUPT_MASK));

        // LCD
        case io::LCD_CONTROL_REGISTER: return emulator.ppu.controlByte;
        case io::LCD_STATUS_REGISTER: return ppu::STAT_UNUSED_BITS | emulator.ppu.lcdStatus.getValue();
        case io::SCROLL_Y_REGISTER: return emulator.ppu.scrollY.getValue();
        case io::SCROLL_X_REGISTER: return emulator.ppu.scrollX.getValue();
        case io::LCD_Y_REGISTER: return emulator.ppu.line.getValue();
        case io::LCD_Y_COMPARE_REGISTER: return emulator.ppu.lyCompare.getValue();
        case io::OAM_DMA_REGISTER: return oamDMARegister;
        case io::BG_PALETTE_REGISTER: return emulator.ppu.bgPalette.getValue();
        case io::OBJ_PALETTE0_REGISTER: return emulator.ppu.spritePalette0.getValue();
        case io::OBJ_PALETTE1_REGISTER: return emulator.ppu.spritePalette1.getValue();
        case io::WINDOW_Y_REGISTER: return emulator.ppu.windowY.getValue();
        case io::WINDOW_X_REGISTER: return emulator.ppu.windowX.getValue();

        // KEY0 reports native CGB CPU mode (0x00). This emulator enters color mode
        // for cartridges whose header advertises CGB support.
        case io::CGB_MODE_REGISTER: return emulator.cgbMode ? 0x00 : io::UNMAPPED_READ_VALUE;

        // KEY1: bit 7 is the current speed and bit 0 is set when a
        // speed switch is armed. The other bits read as 1.
        case io::SPEED_SWITCH_REGISTER: return emulator.cgbMode
            ? static_cast<uint8_t>(0x7E | (emulator.doubleSpeed ? 0x80 : 0) | (emulator.speedSwitchPrepared ? 1 : 0))
            : io::UNMAPPED_READ_VALUE;

        // VBK: only bit 0 is used; the rest read as 1
        case io::VRAM_BANK_REGISTER: return emulator.cgbMode
            ? static_cast<uint8_t>(0xFE | emulator.ppu.getVRAMBank()) : io::UNMAPPED_READ_VALUE;

        // Boot ROM
        case io::BOOT_ROM_DISABLE_REGISTER: return disableBootROMSwitch.getValue();

        // HDMA. Bits 0-4 of HDMA3 hold the destination's
        // offset into VRAM; the upper bits read as 1.
        case io::HDMA_SOURCE_HIGH_REGISTER: return emulator.cgbMode ? static_cast<uint8_t>(hdmaSource >> 8) : io::UNMAPPED_READ_VALUE;
        case io::HDMA_SOURCE_LOW_REGISTER: return emulator.cgbMode ? static_cast<uint8_t>(hdmaSource & mmu::HDMA_ADDRESS_LOW_MASK) : io::UNMAPPED_READ_VALUE;
        case io::HDMA_DESTINATION_HIGH_REGISTER: return emulator.cgbMode
            ? static_cast<uint8_t>(0xE0 | ((hdmaDestination >> 8) & mmu::HDMA_DESTINATION_HIGH_MASK)) : io::UNMAPPED_READ_VALUE;
        case io::HDMA_DESTINATION_LOW_REGISTER: return emulator.cgbMode
            ? static_cast<uint8_t>(hdmaDestination & mmu::HDMA_ADDRESS_LOW_MASK) : io::UNMAPPED_READ_VALUE;

        // HDMA5: bit 7 reads 0 while an HBlank transfer is running, and
        // bits 0-6 hold the blocks left minus one (0x7F once finished)
        case io::HDMA_CONTROL_REGISTER: return emulator.cgbMode
            ? static_cast<uint8_t>((hdmaHBlankMode && hdmaBlocksRemaining ? 0 : 0x80) |
                                   (hdmaBlocksRemaining ? hdmaBlocksRemaining - 1 : mmu::HDMA_LENGTH_MASK))
            : io::UNMAPPED_READ_VALUE;

        // CGB palette index registers can always be read; palette data
        // is locked while the PPU is drawing from VRAM
        case io::BG_PALETTE_INDEX_REGISTER:
        case io::OBJ_PALETTE_INDEX_REGISTER:
            return emulator.cgbMode ? emulator.ppu.readCGBPaletteRegister(address.getValue()) : io::UNMAPPED_READ_VALUE;
        case io::BG_PALETTE_DATA_REGISTER:
        case io::OBJ_PALETTE_DATA_REGISTER:
            if (!emulator.cgbMode || !emulator.ppu.cpuCanAccessVRAM()) return io::UNMAPPED_READ_VALUE;
            return emulator.ppu.readCGBPaletteRegister(address.getValue());

        // OPRI: only bit 0 is used; the rest read as 1
        case io::OBJ_PRIORITY_MODE_REGISTER: return emulator.cgbMode
            ? static_cast<uint8_t>(0xFE | emulator.ppu.getObjectPriorityMode()) : io::UNMAPPED_READ_VALUE;

        // SVBK: only bits 0-2 are used; the rest read as 1
        case io::WRAM_BANK_REGISTER: return emulator.cgbMode ? static_cast<uint8_t>(0xF8 | workRAMBank) : io::UNMAPPED_READ_VALUE;

        // Unused and undocumented registers
        default: return io::UNMAPPED_READ_VALUE;
    }
}

void MMU::write(const Address &address, const uint8_t value, bool bypassPPURestrictions)
{
    // CPU writes that conflict with a running OAM DMA are lost
    if (!bypassPPURestrictions && oamDMABlocksCPU(address.getValue())) return;

    // Cartridge ROM
    if (address.inRange(mmu::CARTRIDGE_ROM_ADDRESS_START, mmu::CARTRIDGE_ROM_ADDRESS_END))
    {
        emulator.cartridge->write(address, value);
        return;
    }

    // VRAM
    if (address.inRange(mmu::VRAM_ADDRESS_START, mmu::VRAM_ADDRESS_END))
    {
        if (!bypassPPURestrictions && !emulator.ppu.cpuCanAccessVRAM()) return;
        emulator.ppu.write(address.getValue() - mmu::VRAM_ADDRESS_START, value);
        return;
    }

    // Cartridge RAM (external)
    if (address.inRange(mmu::CARTRIDGE_RAM_ADDRESS_START, mmu::CARTRIDGE_RAM_ADDRESS_END))
    {
        emulator.cartridge->write(address, value);
        return;
    }

    // WRAM
    if (address.inRange(mmu::WRAM_ADDRESS_START, mmu::WRAM_ADDRESS_END))
    {
        const uint16_t offset = address.getValue() - mmu::WRAM_OFFSET;

        // The second 4 KB of work RAM is switchable on the CGB
        if (emulator.cgbMode && offset >= mmu::WRAM_BANK_SIZE)
            workRAM.at(static_cast<size_t>(workRAMBank) * mmu::WRAM_BANK_SIZE + (offset - mmu::WRAM_BANK_SIZE)) = value;
        else
            workRAM.at(offset) = value;
        return;
    }

    // Echo RAM
    if (address.inRange(mmu::ECHO_RAM_ADDRESS_START, mmu::ECHO_RAM_ADDRESS_END))
    {
        write(address.getValue() - mmu::ECHO_RAM_OFFSET, value, bypassPPURestrictions);
        return;
    }

    // OAM
    if (address.inRange(mmu::OAM_ADDRESS_START, mmu::OAM_ADDRESS_END))
    {
        if (!bypassPPURestrictions && !emulator.ppu.cpuCanAccessOAM()) return;
        oamRAM.at(address.getValue() - mmu::OAM_OFFSET) = value;
        return;
    }

    // Reading from unsuable memory
    if (address.inRange(mmu::UNUSABLE_ADDRESS_START, mmu::UNUSABLE_ADDRESS_END))
    {
        //std::cerr << "Writing to unusable memory: " << std::hex << address.getValue() << "\n";
        return;
    }

    // I/O Registers
    if (address.inRange(mmu::IO_ADDRESS_START, mmu::IO_ADDRESS_END))
    {
        writeIO(address, value);
        return;
    }

    // HRAM
    if (address.inRange(mmu::HRAM_ADDRESS_START, mmu::HRAM_ADDRESS_END))
    {
        highRAM.at(address.getValue() - mmu::HRAM_OFFSET) = value;
        return;
    }

    // Interrupt Enable Register
    if (address == mmu::INTERRUPT_ENABLE_REGISTER)
    {
        emulator.cpu.interruptEnabled.set(value & cpu::INTERRUPT_MASK);
    }
    return;
}

void MMU::writeIO(const Address &address, const uint8_t value)
{
    // Audio registers and wave RAM, including the
    // unused addresses between them
    if (address.inRange(io::AUDIO_ADDRESS_START, io::AUDIO_ADDRESS_END))
    {
        emulator.apu.write(address.getValue(), value);
        return;
    }

    switch (address.getValue())
    {
        case io::JOYPAD_REGISTER:
            if (emulator.sgbMode) emulator.sgb.writeJOYP(value);
            emulator.input.write(value);
            return;

        // Serial data transfer
        case io::SERIAL_DATA_REGISTER: emulator.serial.write(value); return;
        case io::SERIAL_CONTROL_REGISTER: emulator.serial.writeControl(value); return;

        // Timer
        case io::DIVIDER_REGISTER: emulator.timer.resetDivider(); return;
        case io::TIMER_COUNTER_REGISTER: emulator.timer.setTimer(value); return;
        case io::TIMER_MODULO_REGISTER: emulator.timer.setTimerModulo(value); return;
        case io::TIMER_CONTROL_REGISTER: emulator.timer.setTimerControl(value); return;

        // Set CPU interrupt flag
        case io::INTERRUPT_FLAG_REGISTER: emulator.cpu.interruptFlag.set(value & cpu::INTERRUPT_MASK); return;

        // Switch on LCD
        case io::LCD_CONTROL_REGISTER: emulator.ppu.setControlByte(value); return;

        // Only the STAT interrupt enable bits are writable
        case io::LCD_STATUS_REGISTER:
            emulator.ppu.lcdStatus.set((emulator.ppu.lcdStatus.getValue() & ppu::STAT_READ_ONLY_MASK) |
                                       (value & ppu::STAT_INTERRUPT_ENABLE_MASK) | ppu::STAT_UNUSED_BITS);
            return;

        // Vertical Scroll Register
        case io::SCROLL_Y_REGISTER: emulator.ppu.scrollY.set(value); return;

        // Horizontal Scroll Register
        case io::SCROLL_X_REGISTER: emulator.ppu.scrollX.set(value); return;

        // LY - Line Y coordinate
        case io::LCD_Y_REGISTER:
            /* Writing will reset the counter */
            emulator.ppu.line.set(0x0);
            return;

        case io::LCD_Y_COMPARE_REGISTER: emulator.ppu.lyCompare.set(value); return;
        case io::OAM_DMA_REGISTER: requestOAMDMA(value); return;
        case io::BG_PALETTE_REGISTER: emulator.ppu.bgPalette.set(value); return;
        case io::OBJ_PALETTE0_REGISTER: emulator.ppu.spritePalette0.set(value); return;
        case io::OBJ_PALETTE1_REGISTER: emulator.ppu.spritePalette1.set(value); return;
        case io::WINDOW_Y_REGISTER: emulator.ppu.windowY.set(value); return;
        case io::WINDOW_X_REGISTER: emulator.ppu.windowX.set(value); return;

        // KEY1: bit 0 arms a speed switch, which happens on the next STOP
        case io::SPEED_SWITCH_REGISTER:
            if (emulator.cgbMode) emulator.speedSwitchPrepared = (value & 1) != 0;
            return;
        case io::VRAM_BANK_REGISTER:
            if (emulator.cgbMode) emulator.ppu.setVRAMBank(value);
            return;

        // Boot ROM
        case io::BOOT_ROM_DISABLE_REGISTER: disableBootROMSwitch.set(value); return;

        // HDMA source and destination
        case io::HDMA_SOURCE_HIGH_REGISTER:
            if (emulator.cgbMode) hdmaSource = static_cast<uint16_t>((value << 8) | (hdmaSource & mmu::HDMA_ADDRESS_LOW_MASK));
            return;
        case io::HDMA_SOURCE_LOW_REGISTER:
            if (emulator.cgbMode) hdmaSource = static_cast<uint16_t>((hdmaSource & 0xFF00) | (value & mmu::HDMA_ADDRESS_LOW_MASK));
            return;
        case io::HDMA_DESTINATION_HIGH_REGISTER:
            if (emulator.cgbMode)
                hdmaDestination = static_cast<uint16_t>(mmu::VRAM_ADDRESS_START | ((value & mmu::HDMA_DESTINATION_HIGH_MASK) << 8) |
                                                        (hdmaDestination & mmu::HDMA_ADDRESS_LOW_MASK));
            return;
        case io::HDMA_DESTINATION_LOW_REGISTER:
            if (emulator.cgbMode) hdmaDestination = static_cast<uint16_t>((hdmaDestination & 0xFF00) | (value & mmu::HDMA_ADDRESS_LOW_MASK));
            return;

        // HDMA control: start a transfer, or cancel a running HBlank one
        case io::HDMA_CONTROL_REGISTER:
            if (!emulator.cgbMode) return;
            if (hdmaHBlankMode && hdmaBlocksRemaining)
            {
                if (!checkBit(value, mmu::HDMA_HBLANK_MODE_BIT)) hdmaHBlankMode = false;
                return;
            }
            hdmaBlocksRemaining = (value & mmu::HDMA_LENGTH_MASK) + 1;
            hdmaHBlankMode = checkBit(value, mmu::HDMA_HBLANK_MODE_BIT) && emulator.ppu.displayEnabled();
            if (!hdmaHBlankMode)
                while (hdmaBlocksRemaining) transferHDMABlock();
            return;

        case io::BG_PALETTE_INDEX_REGISTER:
        case io::OBJ_PALETTE_INDEX_REGISTER:
            if (emulator.cgbMode) emulator.ppu.writeCGBPaletteRegister(address.getValue(), value);
            return;
        case io::BG_PALETTE_DATA_REGISTER:
        case io::OBJ_PALETTE_DATA_REGISTER:
            if (emulator.cgbMode && emulator.ppu.cpuCanAccessVRAM())
                emulator.ppu.writeCGBPaletteRegister(address.getValue(), value);
            return;

        case io::OBJ_PRIORITY_MODE_REGISTER:
            if (emulator.cgbMode) emulator.ppu.setObjectPriorityMode(value);
            return;

        // Selecting bank 0 maps bank 1
        case io::WRAM_BANK_REGISTER:
            if (emulator.cgbMode) { workRAMBank = value & mmu::WRAM_BANK_MASK; if (workRAMBank == 0) workRAMBank = 1; }
            return;

        // Unused and undocumented registers ignore writes
        default: return;
    }
}

void MMU::requestOAMDMA(const uint8_t value)
{
    oamDMARegister = value;

    // Sources 0xE000-0xFFFF read from the work RAM behind echo RAM
    uint16_t source = static_cast<uint16_t>(value * mmu::DMA_SOURCE_PAGE_SHIFT);
    if (source >= mmu::ECHO_RAM_ADDRESS_START) source -= mmu::ECHO_RAM_OFFSET;
    oamDMAPendingSource = source;

    // The write lands on the last cycle of the instruction, so the
    // startup delay is armed after the rest of this step has run
    oamDMAStartRequested = true;
}

void MMU::copyOAMDMAByte()
{
    oamDMALastByte = read(Address(static_cast<uint16_t>(oamDMASource + oamDMAIndex)), true);
    oamRAM[oamDMAIndex] = oamDMALastByte;
    if (++oamDMAIndex >= mmu::DMA_TRANSFER_LENGTH) oamDMAActive = false;
}

void MMU::tickOAMDMA(const uint32_t mCycles)
{
    for (uint32_t i = 0; i < mCycles; ++i)
    {
        // A transfer that is already running keeps going during a
        // restart's startup delay, so OAM stays locked throughout
        if (oamDMAActive) copyOAMDMAByte();

        if (oamDMAStartDelay && --oamDMAStartDelay == 0)
        {
            oamDMASource = oamDMAPendingSource;
            oamDMAIndex = 0;
            oamDMAActive = true;
        }
    }

    if (oamDMAStartRequested)
    {
        oamDMAStartRequested = false;
        oamDMAStartDelay = mmu::OAM_DMA_START_DELAY;
    }
}

MMU::Bus MMU::busOf(const uint16_t address) const
{
    if (address <= mmu::CARTRIDGE_ROM_ADDRESS_END) return Bus::External;
    if (address <= mmu::VRAM_ADDRESS_END) return Bus::Video;
    if (address <= mmu::CARTRIDGE_RAM_ADDRESS_END) return Bus::External;

    // The CGB has work RAM on its own bus, separate from the cartridge
    if (address <= mmu::ECHO_RAM_ADDRESS_END) return emulator.cgbMode ? Bus::WorkRAM : Bus::External;
    return Bus::None;
}

bool MMU::oamDMABlocksCPU(const uint16_t address) const
{
    if (!oamDMAActive) return false;

    // HRAM, I/O and IE are always reachable
    if (address >= mmu::IO_ADDRESS_START) return false;

    // OAM (and the unusable area after it) belongs to the DMA
    if (address >= mmu::OAM_ADDRESS_START) return true;

    return busOf(address) == busOf(oamDMASource);
}

void MMU::transferHDMABlock()
{
    if (!hdmaBlocksRemaining) return;
    for (uint8_t i = 0; i < mmu::HDMA_BLOCK_SIZE; ++i)
    {
        const uint16_t source = hdmaSource + i;
        const uint16_t destination = hdmaDestination + i;

        // HDMA can only read from cartridge ROM/RAM and work RAM
        const bool validSource = source <= mmu::CARTRIDGE_ROM_ADDRESS_END ||
            (source >= mmu::CARTRIDGE_RAM_ADDRESS_START && source <= mmu::ECHO_RAM_ADDRESS_END);
        const uint8_t data = validSource ? read(Address(source), true) : io::UNMAPPED_READ_VALUE;
        if (destination >= mmu::VRAM_ADDRESS_START && destination <= mmu::VRAM_ADDRESS_END)
            write(Address(destination), data, true);
    }
    hdmaSource += mmu::HDMA_BLOCK_SIZE;

    // The destination wraps around within VRAM
    hdmaDestination = static_cast<uint16_t>(mmu::VRAM_ADDRESS_START |
        ((hdmaDestination + mmu::HDMA_BLOCK_SIZE - mmu::VRAM_ADDRESS_START) & (mmu::VRAM_BANK_SIZE - 1)));
    --hdmaBlocksRemaining;

    // A block takes the same real time in either speed mode,
    // which is twice as many CPU cycles in double speed
    emulator.dmaCyclesRemaining += mmu::HDMA_BLOCK_CYCLES * (emulator.doubleSpeed ? 2 : 1);
    if (!hdmaBlocksRemaining) hdmaHBlankMode = false;
}

uint32_t MMU::hblankDMA()
{
    if (!emulator.cgbMode || !hdmaHBlankMode || !hdmaBlocksRemaining) return 0;
    const uint16_t dmaBefore = emulator.dmaCyclesRemaining;
    transferHDMABlock();
    // Return the M-cycles this block consumed so the PPU can absorb them
    // and keep the HBlank period in sync with the CPU stall.
    return emulator.dmaCyclesRemaining > dmaBefore
        ? emulator.dmaCyclesRemaining - dmaBefore : 0;
}

bool MMU::isBootROMActive() const
{
    return disableBootROMSwitch.getValue() == 0;
}

// The Emulator checks the size before loading it
void MMU::loadBootROM(const std::vector<uint8_t> &rom)
{
    bootROM = rom;
}

// OAM corruption bug
// See https://gbdev.io/pandocs/OAM_Corruption_Bug.html
//
// OAM is treated as 20 rows of four 16-bit little-endian words.

uint16_t MMU::oamWord(int row, int word) const
{
    const size_t index = static_cast<size_t>(row * mmu::OAM_ROW_SIZE + word * 2);
    return static_cast<uint16_t>(oamRAM[index] | (oamRAM[index + 1] << 8));
}

void MMU::setOAMWord(int row, int word, uint16_t value)
{
    const size_t index = static_cast<size_t>(row * mmu::OAM_ROW_SIZE + word * 2);
    oamRAM[index]     = static_cast<uint8_t>(value & 0xFF);
    oamRAM[index + 1] = static_cast<uint8_t>(value >> 8);
}

void MMU::copyOAMRow(int from, int to)
{
    std::copy_n(oamRAM.begin() + from * mmu::OAM_ROW_SIZE, mmu::OAM_ROW_SIZE,
                oamRAM.begin() + to * mmu::OAM_ROW_SIZE);
}

void MMU::corruptOAM(uint16_t address, OAMCorruption kind)
{
    if (address < mmu::OAM_ADDRESS_START || address > mmu::UNUSABLE_ADDRESS_END) return;

    const int row = emulator.ppu.oamScanRow();

    // The first row is never corrupted: every pattern uses the row before it
    if (row <= 0) return;

    if (kind == OAMCorruption::ReadIncrease)
    {
        // Except within the first four rows and on the last row, the row
        // before the preceding one is involved too
        if (row >= 4 && row < mmu::OAM_ROW_COUNT - 1)
        {
            const uint16_t a = oamWord(row - 2, 0);
            const uint16_t b = oamWord(row - 1, 0);
            const uint16_t c = oamWord(row, 0);
            const uint16_t d = oamWord(row - 1, 2);
            setOAMWord(row - 1, 0, static_cast<uint16_t>((b & (a | c | d)) | (a & c & d)));
            copyOAMRow(row - 1, row);
            copyOAMRow(row - 1, row - 2);
        }
        // Followed by a normal read corruption either way
        kind = OAMCorruption::Read;
    }

    const uint16_t a = oamWord(row, 0);
    const uint16_t b = oamWord(row - 1, 0);
    const uint16_t c = oamWord(row - 1, 2);

    const uint16_t first = kind == OAMCorruption::Write
        ? static_cast<uint16_t>(((a ^ c) & (b ^ c)) ^ c)
        : static_cast<uint16_t>(b | (a & c));

    // The rest of the row is copied from the preceding row
    copyOAMRow(row - 1, row);
    setOAMWord(row, 0, first);
}
