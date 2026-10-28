#include "MMU.hpp"
#include "Address.hpp"
#include "Emulator.hpp"
#include "CPU.hpp"
#include "Timer.hpp"
#include "Serial.hpp"
#include "Input.hpp"
#include "BootROM.hpp"
#include "IORegisters.hpp"
#include "Common.hpp"
#include <iostream>

MMU::MMU(Emulator &emulator) :
    emulator(emulator)
{
    workRAM = std::vector<uint8_t>(WRAM_SIZE_BYTES);
    oamRAM = std::vector<uint8_t>(OAM_SIZE_BYTES);
    highRAM = std::vector<uint8_t>(HRAM_SIZE_BYTES);
}

uint8_t MMU::read(const Address &address, bool bypassPPURestrictions) const
{
    // Cartridge ROM
    if (address.inRange(CARTRIDGE_ROM_ADDRESS_START, CARTRIDGE_ROM_ADDRESS_END))
    {
        const uint16_t value = address.getValue();
        const bool inCGBBootROM = value < DMG_BOOT_ROM_SIZE ||
            (value >= CGB_BOOT_ROM_SECOND_PART_START && value < CGB_BOOT_ROM_SIZE);
        if (emulator.cgbMode && !cgbBootROM.empty() && inCGBBootROM && isBootROMActive())
            return cgbBootROM[value];
        if (value < DMG_BOOT_ROM_SIZE && isBootROMActive()) return BOOT_ROM[value];
        return emulator.cartridge->read(address);
    }

    // VRAM
    if (address.inRange(VRAM_ADDRESS_START, VRAM_ADDRESS_END))
    {
        if (!bypassPPURestrictions && !emulator.ppu.cpuCanAccessVRAM()) return UNMAPPED_READ_VALUE;
        return emulator.ppu.read(address.getValue() - VRAM_ADDRESS_START);
    }

    // Cartridge RAM (external)
    if (address.inRange(CARTRIDGE_RAM_ADDRESS_START, CARTRIDGE_RAM_ADDRESS_END))
    {
        return emulator.cartridge->read(address);
    }

    // WRAM
    if (address.inRange(WRAM_ADDRESS_START, WRAM_ADDRESS_END))
    {
        const uint16_t offset = address.getValue() - WRAM_OFFSET;

        // The second 4 KB of work RAM is switchable on the CGB
        if (emulator.cgbMode && offset >= WRAM_BANK_SIZE)
            return workRAM.at(static_cast<size_t>(workRAMBank) * WRAM_BANK_SIZE + (offset - WRAM_BANK_SIZE));
        return workRAM.at(offset);
    }

    // Echo RAM
    if (address.inRange(ECHO_RAM_ADDRESS_START, ECHO_RAM_ADDRESS_END))
    {
        return read(address.getValue() - ECHO_RAM_OFFSET, bypassPPURestrictions);
    }

    // OAM
    if (address.inRange(OAM_ADDRESS_START, OAM_ADDRESS_END))
    {
        if (!bypassPPURestrictions && !emulator.ppu.cpuCanAccessOAM()) return UNMAPPED_READ_VALUE;
        return oamRAM.at(address.getValue() - OAM_OFFSET);
    }

    // Reading from unsuable memory
    if (address.inRange(UNUSABLE_ADDRESS_START, UNUSABLE_ADDRESS_END))
    {
        std::cerr << "Reading from unusable memory: " << std::hex << address.getValue() << "\n";
        return UNMAPPED_READ_VALUE;
    }

    // I/O Registers
    if (address.inRange(IO_ADDRESS_START, IO_ADDRESS_END))
    {
        return readIO(address);
    }

    // HRAM
    if (address.inRange(HRAM_ADDRESS_START, HRAM_ADDRESS_END))
    {
        return highRAM.at(address.getValue() - HRAM_OFFSET);
    }

    // Interrupt Enable Register
    if (address == INTERRUPT_ENABLE_REGISTER)
    {
        // The unused upper bits read as 1
        return static_cast<uint8_t>(~INTERRUPT_MASK | (emulator.cpu.interruptEnabled.getValue() & INTERRUPT_MASK));
    }
    return UNMAPPED_READ_VALUE;
}

uint8_t MMU::readIO(const Address &address) const
{
    // Audio registers and wave RAM, including the
    // unused addresses between them
    if (address.inRange(AUDIO_ADDRESS_START, AUDIO_ADDRESS_END))
    {
        return emulator.apu.read(address.getValue());
    }

    switch (address.getValue())
    {
        case JOYPAD_REGISTER:
        {
            uint8_t input = emulator.input.getInput();

            // With neither half of the joypad selected, the
            // Super Game Boy reports the current player instead
            if (emulator.sgbMode && (input & JOYPAD_SELECT_MASK) == JOYPAD_SELECT_MASK)
                input = static_cast<uint8_t>((input & ~JOYPAD_KEYS_MASK) | emulator.sgb.getPlayerId());
            return input;
        }
        case SERIAL_DATA_REGISTER: return emulator.serial.read();
        case SERIAL_CONTROL_REGISTER: return UNMAPPED_READ_VALUE;

        // Timer. Only the low 3 bits of TAC are used;
        // the rest read as 1.
        case DIVIDER_REGISTER: return emulator.timer.getDivider();
        case TIMER_COUNTER_REGISTER: return emulator.timer.getTimer();
        case TIMER_MODULO_REGISTER: return emulator.timer.getTimerModulo();
        case TIMER_CONTROL_REGISTER: return 0xF8 | emulator.timer.getTimerControl();

        // The unused upper bits of IF read as 1
        case INTERRUPT_FLAG_REGISTER:
            return static_cast<uint8_t>(~INTERRUPT_MASK | (emulator.cpu.interruptFlag.getValue() & INTERRUPT_MASK));

        // LCD
        case LCD_CONTROL_REGISTER: return emulator.ppu.controlByte;
        case LCD_STATUS_REGISTER: return STAT_UNUSED_BITS | emulator.ppu.lcdStatus.getValue();
        case SCROLL_Y_REGISTER: return emulator.ppu.scrollY.getValue();
        case SCROLL_X_REGISTER: return emulator.ppu.scrollX.getValue();
        case LCD_Y_REGISTER: return emulator.ppu.line.getValue();
        case LCD_Y_COMPARE_REGISTER: return emulator.ppu.lyCompare.getValue();
        case OAM_DMA_REGISTER: return UNMAPPED_READ_VALUE;
        case BG_PALETTE_REGISTER: return emulator.ppu.bgPalette.getValue();
        case OBJ_PALETTE0_REGISTER: return emulator.ppu.spritePalette0.getValue();
        case OBJ_PALETTE1_REGISTER: return emulator.ppu.spritePalette1.getValue();
        case WINDOW_Y_REGISTER: return emulator.ppu.windowY.getValue();
        case WINDOW_X_REGISTER: return emulator.ppu.windowX.getValue();

        // KEY0 reports native CGB CPU mode (0x00). This emulator enters color mode
        // for cartridges whose header advertises CGB support.
        case CGB_MODE_REGISTER: return emulator.cgbMode ? 0x00 : UNMAPPED_READ_VALUE;

        // KEY1: bit 7 is the current speed and bit 0 is set when a
        // speed switch is armed. The other bits read as 1.
        case SPEED_SWITCH_REGISTER: return emulator.cgbMode
            ? static_cast<uint8_t>(0x7E | (emulator.doubleSpeed ? 0x80 : 0) | (emulator.speedSwitchPrepared ? 1 : 0))
            : UNMAPPED_READ_VALUE;

        // VBK: only bit 0 is used; the rest read as 1
        case VRAM_BANK_REGISTER: return emulator.cgbMode
            ? static_cast<uint8_t>(0xFE | emulator.ppu.getVRAMBank()) : UNMAPPED_READ_VALUE;

        // Boot ROM
        case BOOT_ROM_DISABLE_REGISTER: return disableBootROMSwitch.getValue();

        // HDMA. Bits 0-4 of HDMA3 hold the destination's
        // offset into VRAM; the upper bits read as 1.
        case HDMA_SOURCE_HIGH_REGISTER: return emulator.cgbMode ? static_cast<uint8_t>(hdmaSource >> 8) : UNMAPPED_READ_VALUE;
        case HDMA_SOURCE_LOW_REGISTER: return emulator.cgbMode ? static_cast<uint8_t>(hdmaSource & HDMA_ADDRESS_LOW_MASK) : UNMAPPED_READ_VALUE;
        case HDMA_DESTINATION_HIGH_REGISTER: return emulator.cgbMode
            ? static_cast<uint8_t>(0xE0 | ((hdmaDestination >> 8) & HDMA_DESTINATION_HIGH_MASK)) : UNMAPPED_READ_VALUE;
        case HDMA_DESTINATION_LOW_REGISTER: return emulator.cgbMode
            ? static_cast<uint8_t>(hdmaDestination & HDMA_ADDRESS_LOW_MASK) : UNMAPPED_READ_VALUE;

        // HDMA5: bit 7 reads 0 while an HBlank transfer is running, and
        // bits 0-6 hold the blocks left minus one (0x7F once finished)
        case HDMA_CONTROL_REGISTER: return emulator.cgbMode
            ? static_cast<uint8_t>((hdmaHBlank && hdmaBlocksRemaining ? 0 : 0x80) |
                                   (hdmaBlocksRemaining ? hdmaBlocksRemaining - 1 : HDMA_LENGTH_MASK))
            : UNMAPPED_READ_VALUE;

        // CGB palette index registers can always be read; palette data
        // is locked while the PPU is drawing from VRAM
        case BG_PALETTE_INDEX_REGISTER:
        case OBJ_PALETTE_INDEX_REGISTER:
            return emulator.cgbMode ? emulator.ppu.readCGBPaletteRegister(address.getValue()) : UNMAPPED_READ_VALUE;
        case BG_PALETTE_DATA_REGISTER:
        case OBJ_PALETTE_DATA_REGISTER:
            if (!emulator.cgbMode || !emulator.ppu.cpuCanAccessVRAM()) return UNMAPPED_READ_VALUE;
            return emulator.ppu.readCGBPaletteRegister(address.getValue());

        // OPRI: only bit 0 is used; the rest read as 1
        case OBJ_PRIORITY_MODE_REGISTER: return emulator.cgbMode
            ? static_cast<uint8_t>(0xFE | emulator.ppu.getObjectPriorityMode()) : UNMAPPED_READ_VALUE;

        // SVBK: only bits 0-2 are used; the rest read as 1
        case WRAM_BANK_REGISTER: return emulator.cgbMode ? static_cast<uint8_t>(0xF8 | workRAMBank) : UNMAPPED_READ_VALUE;

        // Unused and undocumented registers
        default: return UNMAPPED_READ_VALUE;
    }
}

void MMU::write(const Address &address, const uint8_t value, bool bypassPPURestrictions)
{
    // Cartridge ROM
    if (address.inRange(CARTRIDGE_ROM_ADDRESS_START, CARTRIDGE_ROM_ADDRESS_END))
    {
        emulator.cartridge->write(address, value);
        return;
    }

    // VRAM
    if (address.inRange(VRAM_ADDRESS_START, VRAM_ADDRESS_END))
    {
        if (!bypassPPURestrictions && !emulator.ppu.cpuCanAccessVRAM()) return;
        emulator.ppu.write(address.getValue() - VRAM_ADDRESS_START, value);
        return;
    }

    // Cartridge RAM (external)
    if (address.inRange(CARTRIDGE_RAM_ADDRESS_START, CARTRIDGE_RAM_ADDRESS_END))
    {
        emulator.cartridge->write(address, value);
        return;
    }

    // WRAM
    if (address.inRange(WRAM_ADDRESS_START, WRAM_ADDRESS_END))
    {
        const uint16_t offset = address.getValue() - WRAM_OFFSET;

        // The second 4 KB of work RAM is switchable on the CGB
        if (emulator.cgbMode && offset >= WRAM_BANK_SIZE)
            workRAM.at(static_cast<size_t>(workRAMBank) * WRAM_BANK_SIZE + (offset - WRAM_BANK_SIZE)) = value;
        else
            workRAM.at(offset) = value;
        return;
    }

    // Echo RAM
    if (address.inRange(ECHO_RAM_ADDRESS_START, ECHO_RAM_ADDRESS_END))
    {
        write(address.getValue() - ECHO_RAM_OFFSET, value, bypassPPURestrictions);
        return;
    }

    // OAM
    if (address.inRange(OAM_ADDRESS_START, OAM_ADDRESS_END))
    {
        if (!bypassPPURestrictions && !emulator.ppu.cpuCanAccessOAM()) return;
        oamRAM.at(address.getValue() - OAM_OFFSET) = value;
        return;
    }

    // Reading from unsuable memory
    if (address.inRange(UNUSABLE_ADDRESS_START, UNUSABLE_ADDRESS_END))
    {
        //std::cerr << "Writing to unusable memory: " << std::hex << address.getValue() << "\n";
        return;
    }

    // I/O Registers
    if (address.inRange(IO_ADDRESS_START, IO_ADDRESS_END))
    {
        writeIO(address, value);
        return;
    }

    // HRAM
    if (address.inRange(HRAM_ADDRESS_START, HRAM_ADDRESS_END))
    {
        highRAM.at(address.getValue() - HRAM_OFFSET) = value;
        return;
    }

    // Interrupt Enable Register
    if (address == INTERRUPT_ENABLE_REGISTER)
    {
        emulator.cpu.interruptEnabled.set(value & INTERRUPT_MASK);
    }
    return;
}

void MMU::writeIO(const Address &address, const uint8_t value)
{
    // Audio registers and wave RAM, including the
    // unused addresses between them
    if (address.inRange(AUDIO_ADDRESS_START, AUDIO_ADDRESS_END))
    {
        emulator.apu.write(address.getValue(), value);
        return;
    }

    switch (address.getValue())
    {
        case JOYPAD_REGISTER:
            if (emulator.sgbMode) emulator.sgb.writeJOYP(value);
            emulator.input.write(value);
            return;

        // Serial data transfer
        case SERIAL_DATA_REGISTER: emulator.serial.write(value); return;
        case SERIAL_CONTROL_REGISTER: emulator.serial.writeControl(value); return;

        // Timer
        case DIVIDER_REGISTER: emulator.timer.resetDivider(); return;
        case TIMER_COUNTER_REGISTER: emulator.timer.setTimer(value); return;
        case TIMER_MODULO_REGISTER: emulator.timer.setTimerModulo(value); return;
        case TIMER_CONTROL_REGISTER: emulator.timer.setTimerControl(value); return;

        // Set CPU interrupt flag
        case INTERRUPT_FLAG_REGISTER: emulator.cpu.interruptFlag.set(value & INTERRUPT_MASK); return;

        // Switch on LCD
        case LCD_CONTROL_REGISTER: emulator.ppu.setControlByte(value); return;

        // Only the STAT interrupt enable bits are writable
        case LCD_STATUS_REGISTER:
            emulator.ppu.lcdStatus.set((emulator.ppu.lcdStatus.getValue() & STAT_READ_ONLY_MASK) |
                                       (value & STAT_INTERRUPT_ENABLE_MASK) | STAT_UNUSED_BITS);
            return;

        // Vertical Scroll Register
        case SCROLL_Y_REGISTER: emulator.ppu.scrollY.set(value); return;

        // Horizontal Scroll Register
        case SCROLL_X_REGISTER: emulator.ppu.scrollX.set(value); return;

        // LY - Line Y coordinate
        case LCD_Y_REGISTER:
            /* Writing will reset the counter */
            emulator.ppu.line.set(0x0);
            return;

        case LCD_Y_COMPARE_REGISTER: emulator.ppu.lyCompare.set(value); return;
        case OAM_DMA_REGISTER: dmaTransfer(value); return;
        case BG_PALETTE_REGISTER: emulator.ppu.bgPalette.set(value); return;
        case OBJ_PALETTE0_REGISTER: emulator.ppu.spritePalette0.set(value); return;
        case OBJ_PALETTE1_REGISTER: emulator.ppu.spritePalette1.set(value); return;
        case WINDOW_Y_REGISTER: emulator.ppu.windowY.set(value); return;
        case WINDOW_X_REGISTER: emulator.ppu.windowX.set(value); return;

        // KEY1: bit 0 arms a speed switch, which happens on the next STOP
        case SPEED_SWITCH_REGISTER:
            if (emulator.cgbMode) emulator.speedSwitchPrepared = (value & 1) != 0;
            return;
        case VRAM_BANK_REGISTER:
            if (emulator.cgbMode) emulator.ppu.setVRAMBank(value);
            return;

        // Boot ROM
        case BOOT_ROM_DISABLE_REGISTER: disableBootROMSwitch.set(value); return;

        // HDMA source and destination
        case HDMA_SOURCE_HIGH_REGISTER:
            if (emulator.cgbMode) hdmaSource = static_cast<uint16_t>((value << 8) | (hdmaSource & HDMA_ADDRESS_LOW_MASK));
            return;
        case HDMA_SOURCE_LOW_REGISTER:
            if (emulator.cgbMode) hdmaSource = static_cast<uint16_t>((hdmaSource & 0xFF00) | (value & HDMA_ADDRESS_LOW_MASK));
            return;
        case HDMA_DESTINATION_HIGH_REGISTER:
            if (emulator.cgbMode)
                hdmaDestination = static_cast<uint16_t>(VRAM_ADDRESS_START | ((value & HDMA_DESTINATION_HIGH_MASK) << 8) |
                                                        (hdmaDestination & HDMA_ADDRESS_LOW_MASK));
            return;
        case HDMA_DESTINATION_LOW_REGISTER:
            if (emulator.cgbMode) hdmaDestination = static_cast<uint16_t>((hdmaDestination & 0xFF00) | (value & HDMA_ADDRESS_LOW_MASK));
            return;

        // HDMA control: start a transfer, or cancel a running HBlank one
        case HDMA_CONTROL_REGISTER:
            if (!emulator.cgbMode) return;
            if (hdmaHBlank && hdmaBlocksRemaining)
            {
                if (!checkBit(value, HDMA_HBLANK_MODE_BIT)) hdmaHBlank = false;
                return;
            }
            hdmaBlocksRemaining = (value & HDMA_LENGTH_MASK) + 1;
            hdmaHBlank = checkBit(value, HDMA_HBLANK_MODE_BIT) && emulator.ppu.displayEnabled();
            if (!hdmaHBlank)
                while (hdmaBlocksRemaining) transferHDMABlock();
            return;

        case BG_PALETTE_INDEX_REGISTER:
        case OBJ_PALETTE_INDEX_REGISTER:
            if (emulator.cgbMode) emulator.ppu.writeCGBPaletteRegister(address.getValue(), value);
            return;
        case BG_PALETTE_DATA_REGISTER:
        case OBJ_PALETTE_DATA_REGISTER:
            if (emulator.cgbMode && emulator.ppu.cpuCanAccessVRAM())
                emulator.ppu.writeCGBPaletteRegister(address.getValue(), value);
            return;

        case OBJ_PRIORITY_MODE_REGISTER:
            if (emulator.cgbMode) emulator.ppu.setObjectPriorityMode(value);
            return;

        // Selecting bank 0 maps bank 1
        case WRAM_BANK_REGISTER:
            if (emulator.cgbMode) { workRAMBank = value & WRAM_BANK_MASK; if (workRAMBank == 0) workRAMBank = 1; }
            return;

        // Unused and undocumented registers ignore writes
        default: return;
    }
}

void MMU::dmaTransfer(const uint8_t value)
{
    uint8_t valueAtAddress;
    Address startAddress = value * DMA_SOURCE_PAGE_SHIFT;

    for (uint8_t i = 0x0; i <= DMA_TRANSFER_LENGTH; i++)
    {
        Address fromAddress = startAddress.getValue() + i;
        Address toAddress = OAM_ADDRESS_START + i;
        valueAtAddress = read(fromAddress, true);
        write(toAddress, valueAtAddress, true);
    }
    emulator.dmaCyclesRemaining = OAM_DMA_CYCLES;
}

void MMU::transferHDMABlock()
{
    if (!hdmaBlocksRemaining) return;
    for (uint8_t i = 0; i < HDMA_BLOCK_SIZE; ++i)
    {
        const uint16_t source = hdmaSource + i;
        const uint16_t destination = hdmaDestination + i;

        // HDMA can only read from cartridge ROM/RAM and work RAM
        const bool validSource = source <= CARTRIDGE_ROM_ADDRESS_END ||
            (source >= CARTRIDGE_RAM_ADDRESS_START && source <= ECHO_RAM_ADDRESS_END);
        const uint8_t data = validSource ? read(Address(source), true) : UNMAPPED_READ_VALUE;
        if (destination >= VRAM_ADDRESS_START && destination <= VRAM_ADDRESS_END)
            write(Address(destination), data, true);
    }
    hdmaSource += HDMA_BLOCK_SIZE;

    // The destination wraps around within VRAM
    hdmaDestination = static_cast<uint16_t>(VRAM_ADDRESS_START |
        ((hdmaDestination + HDMA_BLOCK_SIZE - VRAM_ADDRESS_START) & (VRAM_BANK_SIZE - 1)));
    --hdmaBlocksRemaining;

    // A block takes the same real time in either speed mode,
    // which is twice as many CPU cycles in double speed
    emulator.dmaCyclesRemaining += HDMA_BLOCK_CYCLES * (emulator.doubleSpeed ? 2 : 1);
    if (!hdmaBlocksRemaining) hdmaHBlank = false;
}

void MMU::hblankDMA()
{
    if (emulator.cgbMode && hdmaHBlank && hdmaBlocksRemaining) transferHDMABlock();
}

bool MMU::isBootROMActive() const
{
    return disableBootROMSwitch.getValue() == 0;
}

bool MMU::loadCGBBootROM(const std::vector<uint8_t> &rom)
{
    if (rom.size() != CGB_BOOT_ROM_SIZE)
    {
        std::cerr << "CGB boot ROM must be exactly 0x900 bytes\n";
        return false;
    }
    cgbBootROM = rom;
    return true;
}
