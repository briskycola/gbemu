#include "mmu.hpp"
#include "address.hpp"
#include "emulator.hpp"
#include "cpu.hpp"
#include "timer.hpp"
#include "serial.hpp"
#include "input.hpp"
#include "bootrom.hpp"
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
        if (address.inRange(0x0, 0xFF) && isBootROMActive()) return bootROM[address.getValue()];
        return emulator.cartridge->read(address);
    }

    // VRAM
    if (address.inRange(VRAM_ADDRESS_START, VRAM_ADDRESS_END))
    {
        if (!bypassPPURestrictions && !emulator.ppu.cpuCanAccessVRAM()) return 0xFF;
        return emulator.ppu.read(address.getValue() - 0x8000);
    }

    // Cartridge RAM (external)
    if (address.inRange(CARTRIDGE_RAM_ADDRESS_START, CARTRIDGE_RAM_ADDRESS_END))
    {
        return emulator.cartridge->read(address);
    }

    // WRAM
    if (address.inRange(WRAM_ADDRESS_START, WRAM_ADDRESS_END))
    {
        const uint16_t offset = address.getValue() - 0xC000;
        if (emulator.cgbMode && offset >= 0x1000)
            return workRAM.at(0x1000 + static_cast<size_t>(workRAMBank - 1) * 0x1000 + (offset - 0x1000));
        return workRAM.at(offset);
    }

    // Echo RAM
    if (address.inRange(ECHO_RAM_ADDRESS_START, ECHO_RAM_ADDRESS_END))
    {
        return read(address.getValue() - 0x2000, bypassPPURestrictions);
    }

    // OAM
    if (address.inRange(OAM_ADDRESS_START, OAM_ADDRESS_END))
    {
        if (!bypassPPURestrictions && !emulator.ppu.cpuCanAccessOAM()) return 0xFF;
        return oamRAM.at(address.getValue() - 0xFE00);
    }

    // Reading from unsuable memory
    if (address.inRange(0xFEA0, 0xFEFF))
    {
        std::cerr << "Reading from unusable memory: " << std::hex << address.getValue() << "\n";
        return 0xFF;
    }

    // I/O Registers
    if (address.inRange(IO_ADDRESS_START, IO_ADDRESS_END))
    {
        return readIO(address);
    }

    // HRAM
    if (address.inRange(HRAM_ADDRESS_START, HRAM_ADDRESS_END))
    {
        return highRAM.at(address.getValue() - 0xFF80);
    }

    // Interrupt Enable Register
    if (address == INTERRUPT_ENABLE_REGISTER)
    {
        return 0xE0 | (emulator.cpu.interruptEnabled.getValue() & 0x1F);
    }
    return 0xFF;
}

uint8_t MMU::readIO(const Address &address) const
{
    switch (address.getValue())
    {
        case 0xFF00:
        {
            uint8_t input = emulator.input.getInput();
            if (emulator.sgbMode && (input & 0x30) == 0x30)
                input = static_cast<uint8_t>((input & 0xF0) | emulator.sgb.getPlayerId());
            return input;
        }
        case 0xFF01: return emulator.serial.read();
        case 0xFF02: return 0xFF;
        case 0xFF03: return 0xFF;
        case 0xFF04: return emulator.timer.getDivider();
        case 0xFF05: return emulator.timer.getTimer();
        case 0xFF06: return emulator.timer.getTimerModulo();
        case 0xFF07: return 0xF8 | emulator.timer.getTimerControl();
        case 0xFF08:
        case 0xFF09:
        case 0xFF0A:
        case 0xFF0B:
        case 0xFF0C:
        case 0xFF0D:
        case 0xFF0E: return 0xFF;
        case 0xFF0F: return 0xE0 | (emulator.cpu.interruptFlag.getValue() & 0x1F);

        // Audio channel 1
        case 0xFF10:
        case 0xFF11:
        case 0xFF12:
        case 0xFF13:
        case 0xFF14: return emulator.apu.read(address.getValue());

        case 0xFF15: return 0xFF;

        // Audio channel 2
        case 0xFF16:
        case 0xFF17:
        case 0xFF18:
        case 0xFF19: return emulator.apu.read(address.getValue());

        // Audio channel 3
        case 0xFF1A:
        case 0xFF1B:
        case 0xFF1C:
        case 0xFF1D:
        case 0xFF1E: return emulator.apu.read(address.getValue());
        case 0xFF1F: return 0xFF;

        // Audio channel 4
        case 0xFF20:
        case 0xFF21:
        case 0xFF22:
        case 0xFF23: return emulator.apu.read(address.getValue());

        // Audio master volume, routing, and power
        case 0xFF24: return emulator.apu.read(address.getValue());

        case 0xFF25: return emulator.apu.read(address.getValue());

        case 0xFF26: return emulator.apu.read(address.getValue());

        case 0xFF27:
        case 0xFF28:
        case 0xFF29:
        case 0xFF2A:
        case 0xFF2B:
        case 0xFF2C:
        case 0xFF2D:
        case 0xFF2E:
        case 0xFF2F: return 0xFF;

        // Audio wave pattern RAM
        case 0xFF30:
        case 0xFF31:
        case 0xFF32:
        case 0xFF33:
        case 0xFF34:
        case 0xFF35:
        case 0xFF36:
        case 0xFF37:
        case 0xFF38:
        case 0xFF39:
        case 0xFF3A:
        case 0xFF3B:
        case 0xFF3C:
        case 0xFF3D:
        case 0xFF3E:
        case 0xFF3F: return emulator.apu.read(address.getValue());

        case 0xFF40: return emulator.ppu.controlByte;
        case 0xFF41: return 0x80 | emulator.ppu.lcdStatus.getValue();
        case 0xFF42: return emulator.ppu.scrollY.getValue();
        case 0xFF43: return emulator.ppu.scrollX.getValue();
        case 0xFF44: return emulator.ppu.line.getValue();
        case 0xFF45: return emulator.ppu.lyCompare.getValue();
        case 0xFF46: return 0xFF;
        case 0xFF47: return emulator.ppu.bgPalette.getValue();
        case 0xFF48: return emulator.ppu.spritePalette0.getValue();
        case 0xFF49: return emulator.ppu.spritePalette1.getValue();
        case 0xFF4A: return emulator.ppu.windowY.getValue();
        case 0xFF4B: return emulator.ppu.windowX.getValue();

        // KEY0 reports native CGB CPU mode. This emulator enters color mode
        // for cartridges whose header advertises CGB support.
        case 0xFF4C: return emulator.cgbMode ? 0x00 : 0xFF;
        case 0xFF4D: return emulator.cgbMode
            ? static_cast<uint8_t>(0x7E | (emulator.doubleSpeed ? 0x80 : 0) | (emulator.speedSwitchPrepared ? 1 : 0))
            : 0xFF;
        case 0xFF4E:
        case 0xFF4F: return emulator.cgbMode ? static_cast<uint8_t>(0xFE | emulator.ppu.getVRAMBank()) : 0xFF;

        // Boot ROM
        case 0xFF50: return disableBootROMSwitch.getValue();
        case 0xFF51: return emulator.cgbMode ? static_cast<uint8_t>(hdmaSource >> 8) : 0xFF;
        case 0xFF52: return emulator.cgbMode ? static_cast<uint8_t>(hdmaSource & 0xF0) : 0xFF;
        case 0xFF53: return emulator.cgbMode ? static_cast<uint8_t>(0xE0 | ((hdmaDestination >> 8) & 0x1F)) : 0xFF;
        case 0xFF54: return emulator.cgbMode ? static_cast<uint8_t>(hdmaDestination & 0xF0) : 0xFF;
        case 0xFF55: return emulator.cgbMode
            ? static_cast<uint8_t>((hdmaHBlank && hdmaBlocksRemaining ? 0 : 0x80) | (hdmaBlocksRemaining ? hdmaBlocksRemaining - 1 : 0x7F))
            : 0xFF;
        case 0xFF56: return 0xFF;

        case 0xFF57:
        case 0xFF58:
        case 0xFF59:
        case 0xFF5A:
        case 0xFF5B:
        case 0xFF5C:
        case 0xFF5D:
        case 0xFF5E:
        case 0xFF5F:
        case 0xFF60:
        case 0xFF61:
        case 0xFF62:
        case 0xFF63:
        case 0xFF64:
        case 0xFF65:
        case 0xFF66:
        case 0xFF67: return 0xFF;
            
        case 0xFF68:
        case 0xFF6A:
            return emulator.cgbMode ? emulator.ppu.readCGBPaletteRegister(address.getValue()) : 0xFF;
        case 0xFF69:
        case 0xFF6B:
            if (!emulator.cgbMode || !emulator.ppu.cpuCanAccessVRAM()) return 0xFF;
            return emulator.ppu.readCGBPaletteRegister(address.getValue());

        case 0xFF6C: return emulator.cgbMode
            ? static_cast<uint8_t>(0xFE | emulator.ppu.getObjectPriorityMode()) : 0xFF;
        case 0xFF6D:
        case 0xFF6E:
        case 0xFF6F: return 0xFF;

        // Unimplemented: CGB WRAM bank
        case 0xFF70: return emulator.cgbMode ? static_cast<uint8_t>(0xF8 | workRAMBank) : 0xFF;

        // Unimplemented: Some undocumented registers in this range
        case 0xFF71:
        case 0xFF72:
        case 0xFF73:
        case 0xFF74:
        case 0xFF75:
        case 0xFF76:
        case 0xFF77:
        case 0xFF78:
        case 0xFF79:
        case 0xFF7A:
        case 0xFF7B:
        case 0xFF7C:
        case 0xFF7D:
        case 0xFF7E:
        case 0xFF7F: return 0xFF;

        default: std::runtime_error("Error reading from unmapped I/O address");
    }
    return 0xFF;
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
        emulator.ppu.write(address.getValue() - 0x8000, value);
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
        if (emulator.cgbMode && offset >= 0x1000)
            workRAM.at(0x1000 + static_cast<size_t>(workRAMBank - 1) * 0x1000 + (offset - 0x1000)) = value;
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
        emulator.cpu.interruptEnabled.set(value & 0x1F);
    }
    return;
}

void MMU::writeIO(const Address &address, const uint8_t value)
{
    switch (address.getValue())
    {
        case 0xFF00:
            if (emulator.sgbMode) emulator.sgb.writeJOYP(value);
            emulator.input.write(value);
            return;

        // Serial data transfer (SB)
        case 0xFF01: emulator.serial.write(value); return;

        // Serial data transfer (SB)
        case 0xFF02: emulator.serial.writeControl(value); return;

        case 0xFF03: return;

        // Timer
        case 0xFF04: emulator.timer.resetDivider(); return;
        case 0xFF05: emulator.timer.setTimer(value); return;
        case 0xFF06: emulator.timer.setTimerModulo(value); return;
        case 0xFF07: emulator.timer.setTimerControl(value); return;

        
        case 0xFF08:
        case 0xFF09:
        case 0xFF0A:
        case 0xFF0B:
        case 0xFF0C:
        case 0xFF0D:
        case 0xFF0E: return;

        // Set CPU interrupt flag
        case 0xFF0F: emulator.cpu.interruptFlag.set(value & 0x1F); return;

        // Audio channel registers
        case 0xFF10:
        case 0xFF11:
        case 0xFF12:
        case 0xFF13:
        case 0xFF14: emulator.apu.write(address.getValue(), value); return;

        case 0xFF15: return;

        // Audio channel 2
        case 0xFF16:
        case 0xFF17:
        case 0xFF18:
        case 0xFF19: emulator.apu.write(address.getValue(), value); return;

        // Audio channel 3
        case 0xFF1A:
        case 0xFF1B:
        case 0xFF1C:
        case 0xFF1D:
        case 0xFF1E: emulator.apu.write(address.getValue(), value); return;

        case 0xFF1F: return;

        // Audio channel 4
        case 0xFF20:
        case 0xFF21:
        case 0xFF22:
        case 0xFF23: emulator.apu.write(address.getValue(), value); return;

        // Audio master volume and output routing
        case 0xFF24:
        case 0xFF25:
        case 0xFF26: emulator.apu.write(address.getValue(), value); return;

        case 0xFF27:
        case 0xFF28:
        case 0xFF29:
        case 0xFF2A:
        case 0xFF2B:
        case 0xFF2C:
        case 0xFF2D:
        case 0xFF2E:
        case 0xFF2F: return;

        // Audio wave pattern RAM
        case 0xFF30:
        case 0xFF31:
        case 0xFF32:
        case 0xFF33:
        case 0xFF34:
        case 0xFF35:
        case 0xFF36:
        case 0xFF37:
        case 0xFF38:
        case 0xFF39:
        case 0xFF3A:
        case 0xFF3B:
        case 0xFF3C:
        case 0xFF3D:
        case 0xFF3E:
        case 0xFF3F: emulator.apu.write(address.getValue(), value); return;

        // Switch on LCD
        case 0xFF40: emulator.ppu.setControlByte(value); return;
        case 0xFF41: emulator.ppu.lcdStatus.set((emulator.ppu.lcdStatus.getValue() & 0x07) | (value & 0x78) | 0x80); return;

        // Vertical Scroll Register
        case 0xFF42: emulator.ppu.scrollY.set(value); return;

        // Horizontal Scroll Register
        case 0xFF43: emulator.ppu.scrollX.set(value); return;

        // LY - Line Y coordinate
        case 0xFF44:
            /* Writing will reset the counter */
            emulator.ppu.line.set(0x0);
            return;

        case 0xFF45: emulator.ppu.lyCompare.set(value); return;
        case 0xFF46: dmaTransfer(value); return;
        case 0xFF47: emulator.ppu.bgPalette.set(value); return;
        case 0xFF48: emulator.ppu.spritePalette0.set(value); return;
        case 0xFF49: emulator.ppu.spritePalette1.set(value); return;
        case 0xFF4A: emulator.ppu.windowY.set(value); return;
        case 0xFF4B: emulator.ppu.windowX.set(value); return;

        // Unimplemented: CGB mode behavior
        case 0xFF4C: return;
        case 0xFF4D:
            if (emulator.cgbMode) emulator.speedSwitchPrepared = (value & 1) != 0;
            return;
        case 0xFF4E:
        case 0xFF4F:
            if (emulator.cgbMode) emulator.ppu.setVRAMBank(value & 1);
            return;

        // Boot ROM
        case 0xFF50: disableBootROMSwitch.set(value); return;

        case 0xFF51:
            if (emulator.cgbMode) hdmaSource = static_cast<uint16_t>((value << 8) | (hdmaSource & 0x00F0));
            return;
        case 0xFF52:
            if (emulator.cgbMode) hdmaSource = static_cast<uint16_t>((hdmaSource & 0xFF00) | (value & 0xF0));
            return;
        case 0xFF53:
            if (emulator.cgbMode) hdmaDestination = static_cast<uint16_t>(0x8000 | ((value & 0x1F) << 8) | (hdmaDestination & 0x00F0));
            return;
        case 0xFF54:
            if (emulator.cgbMode) hdmaDestination = static_cast<uint16_t>((hdmaDestination & 0xFF00) | (value & 0xF0));
            return;
        case 0xFF55:
            if (!emulator.cgbMode) return;
            if (hdmaHBlank && hdmaBlocksRemaining)
            {
                if (!(value & 0x80)) hdmaHBlank = false;
                return;
            }
            hdmaBlocksRemaining = (value & 0x7F) + 1;
            hdmaHBlank = (value & 0x80) != 0 && (emulator.ppu.controlByte & 0x80) != 0;
            if (!hdmaHBlank)
                while (hdmaBlocksRemaining) transferHDMABlock();
            return;
        case 0xFF56: return;

        case 0xFF57:
        case 0xFF58:
        case 0xFF59:
        case 0xFF5A:
        case 0xFF5B:
        case 0xFF5C:
        case 0xFF5D:
        case 0xFF5E:
        case 0xFF5F:
        case 0xFF60:
        case 0xFF61:
        case 0xFF62:
        case 0xFF63:
        case 0xFF64:
        case 0xFF65:
        case 0xFF66:
        case 0xFF67: return;

        case 0xFF68:
        case 0xFF6A:
            if (emulator.cgbMode) emulator.ppu.writeCGBPaletteRegister(address.getValue(), value);
            return;
        case 0xFF69:
        case 0xFF6B:
            if (emulator.cgbMode && emulator.ppu.cpuCanAccessVRAM())
                emulator.ppu.writeCGBPaletteRegister(address.getValue(), value);
            return;

        case 0xFF6C:
            if (emulator.cgbMode) emulator.ppu.setObjectPriorityMode(value);
            return;

        case 0xFF6D:
        case 0xFF6E:
        case 0xFF6F: return;

        /* Unimplemented: CGB WRAM bank */
        case 0xFF70:
            if (emulator.cgbMode) { workRAMBank = value & 7; if (workRAMBank == 0) workRAMBank = 1; }
            return;

        /* Unimplemented: Some undocumented registers in this range */
        case 0xFF71:
        case 0xFF72:
        case 0xFF73:
        case 0xFF74:
        case 0xFF75:
        case 0xFF76:
        case 0xFF77:
        case 0xFF78:
        case 0xFF79:
        case 0xFF7A:
        case 0xFF7B:
        case 0xFF7C:
        case 0xFF7D:
        case 0xFF7E:
        case 0xFF7F: return;

        default: std::runtime_error("Can't write to unmapped I/O address");
    }
    return;
}

void MMU::dmaTransfer(const uint8_t value)
{
    uint8_t valueAtAddress;
    Address startAddress = value * 0x100;

    for (uint8_t i = 0x0; i <= 0x9F; i++)
    {
        Address fromAddress = startAddress.getValue() + i;
        Address toAddress = 0xFE00 + i;
        valueAtAddress = read(fromAddress, true);
        write(toAddress, valueAtAddress, true);
    }
    // A DMG OAM DMA transfer occupies the CPU bus for 160 machine cycles.
    emulator.dmaCyclesRemaining = 160;
}

void MMU::transferHDMABlock()
{
    if (!hdmaBlocksRemaining) return;
    for (uint8_t i = 0; i < 0x10; ++i)
    {
        const uint16_t source = hdmaSource + i;
        const uint16_t destination = hdmaDestination + i;
        const uint8_t data = source <= 0x7FFF || (source >= 0xA000 && source <= 0xFDFF)
            ? read(Address(source), true) : 0xFF;
        if (destination >= 0x8000 && destination <= 0x9FFF)
            write(Address(destination), data, true);
    }
    hdmaSource += 0x10;
    hdmaDestination = static_cast<uint16_t>(0x8000 | ((hdmaDestination + 0x10 - 0x8000) & 0x1FFF));
    --hdmaBlocksRemaining;
    emulator.dmaCyclesRemaining += emulator.doubleSpeed ? 16 : 8;
    if (!hdmaBlocksRemaining) hdmaHBlank = false;
}

void MMU::hblankDMA()
{
    if (emulator.cgbMode && hdmaHBlank && hdmaBlocksRemaining) transferHDMABlock();
}

bool MMU::isBootROMActive() const
{
    //return read(0xFF50) != 0x1;
    return disableBootROMSwitch.getValue() == 0;
}
