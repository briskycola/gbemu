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

uint8_t MMU::read(const Address &address) const
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
        return workRAM.at(address.getValue() - 0xC000);
    }

    // Echo RAM
    if (address.inRange(ECHO_RAM_ADDRESS_START, ECHO_RAM_ADDRESS_END))
    {
        return read(address.getValue() - 0x2000);
    }

    // OAM
    if (address.inRange(OAM_ADDRESS_START, OAM_ADDRESS_END))
    {
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
        return emulator.cpu.interruptEnabled.getValue();
    }
    return 0xFF;
}

uint8_t MMU::readIO(const Address &address) const
{
    switch (address.getValue())
    {
        case 0xFF00: return emulator.input.getInput();
        case 0xFF01: return emulator.serial.read();
        case 0xFF02: return 0xFF;
        case 0xFF03: return 0xFF;
        case 0xFF04: return emulator.timer.getDivider();
        case 0xFF05: return emulator.timer.getTimer();
        case 0xFF06: return emulator.timer.getTimerModulo();
        case 0xFF07: return emulator.timer.getTimerControl();
        case 0xFF08:
        case 0xFF09:
        case 0xFF0A:
        case 0xFF0B:
        case 0xFF0C:
        case 0xFF0D:
        case 0xFF0E: return 0xFF;
        case 0xFF0F: return emulator.cpu.interruptFlag.getValue();

        // Unimplemented: Audio - Channel 1: Tone & Sweep
        case 0xFF10:
        case 0xFF11:
        case 0xFF12:
        case 0xFF13:
        case 0xFF14: return 0xFF;

        case 0xFF15: return 0xFF;

        // Unimplemented: Audio - Channel 2: Tone
        case 0xFF16:
        case 0xFF17:
        case 0xFF18:
        case 0xFF19: return 0xFF;

        // Unimplemented: Audio - Channel 3: Wave Output
        case 0xFF1A:
        case 0xFF1B:
        case 0xFF1C:
        case 0xFF1D:
        case 0xFF1E:
        case 0xFF1F: return 0xFF;

        // Unimplemented: Audio - Channel 4: Noise
        case 0xFF20:
        case 0xFF21:
        case 0xFF22:
        case 0xFF23: return 0xFF;

        // Unimplemented: Audio - Channel control/ON-OFF/Volume
        case 0xFF24: return 0xFF;

        // Unimplemented: Audio - Selection of sound output terminal
        case 0xFF25: return 0xFF;

        // Unimplemented: Audio - Sound on/off
        case 0xFF26: return 0xFF;

        case 0xFF27:
        case 0xFF28:
        case 0xFF29:
        case 0xFF2A:
        case 0xFF2B:
        case 0xFF2C:
        case 0xFF2D:
        case 0xFF2E:
        case 0xFF2F: return 0xFF;

        // Unimplemented: Audio - Wave pattern RAM
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
        case 0xFF3F: return 0xFF;

        case 0xFF40: return emulator.ppu.controlByte;
        case 0xFF41: return emulator.ppu.lcdStatus.getValue();
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

        // Unimplemented: CGB mode behavior
        case 0xFF4C: return 0xFF;
        case 0xFF4D: return 0xFF;
        case 0xFF4E:
        case 0xFF4F: return 0xFF;

        // Boot ROM
        case 0xFF50: return disableBootROMSwitch.getValue();
        case 0xFF51: return 0xFF;
        case 0xFF52: return 0xFF;
        case 0xFF53: return 0xFF;
        case 0xFF54: return 0xFF;
        case 0xFF55: return 0xFF;
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
            
        // Unimplemented: Background color palette spec/index
        case 0xFF68: return 0xFF;
            
        // Unimplemented: Background color palette data
        case 0xFF69: return 0xFF;

        // Unimplemented: OBJ color palette spec/index
        case 0xFF6A: return 0xFF;

        // Unimplemented: OBJ color palette data
        case 0xFF6B: return 0xFF;

        // Unimplemented: Object priority mode
        case 0xFF6C: return 0xFF;
        case 0xFF6D:
        case 0xFF6E:
        case 0xFF6F: return 0xFF;

        // Unimplemented: CGB WRAM bank
        case 0xFF70: return 0xFF;

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

void MMU::write(const Address &address, const uint8_t value)
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
        workRAM.at(address.getValue() - WRAM_OFFSET) = value;
        return;
    }

    // Echo RAM
    if (address.inRange(ECHO_RAM_ADDRESS_START, ECHO_RAM_ADDRESS_END))
    {
        write(address.getValue() - ECHO_RAM_OFFSET, value);
        return;
    }

    // OAM
    if (address.inRange(OAM_ADDRESS_START, OAM_ADDRESS_END))
    {
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
        emulator.cpu.interruptEnabled.set(value);
    }
    return;
}

void MMU::writeIO(const Address &address, const uint8_t value)
{
    switch (address.getValue())
    {
        case 0xFF00: emulator.input.write(value); return;

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
        case 0xFF0F: emulator.cpu.interruptFlag.set(value); return;

        // Unimplemented: Audio - Channel 1: Tone & Sweep
        case 0xFF10:
        case 0xFF11:
        case 0xFF12:
        case 0xFF13:
        case 0xFF14: return;

        case 0xFF15: return;

        // Unimplemented: Audio - Channel 2: Tone
        case 0xFF16:
        case 0xFF17:
        case 0xFF18:
        case 0xFF19: return;

        // Unimplemented: Audio - Channel 3: Wave Output
        case 0xFF1A:
        case 0xFF1B:
        case 0xFF1C:
        case 0xFF1D:
        case 0xFF1E: return;

        case 0xFF1F: return;

        // Unimplemented: Audio - Channel 4: Noise
        case 0xFF20:
        case 0xFF21:
        case 0xFF22:
        case 0xFF23: return;

        // Unimplemented: Audio - Channel control/ON-OFF/Volume
        case 0xFF24: return;

        // Unimplemented: Audio - Selection of sound output terminal
        case 0xFF25: return;

        // Unimplemented: Audio - Sound on/off
        case 0xFF26: return;

        case 0xFF27:
        case 0xFF28:
        case 0xFF29:
        case 0xFF2A:
        case 0xFF2B:
        case 0xFF2C:
        case 0xFF2D:
        case 0xFF2E:
        case 0xFF2F: return;

        // Unimplemented: Audio - Wave pattern RAM
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
        case 0xFF3F: return;

        // Switch on LCD
        case 0xFF40: emulator.ppu.controlByte = value; return;
        case 0xFF41: emulator.ppu.lcdStatus.set(value); return;

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
        case 0xFF4D: return;
        case 0xFF4E:
        case 0xFF4F: return;

        // Boot ROM
        case 0xFF50: disableBootROMSwitch.set(value); return;

        case 0xFF51: return;
        case 0xFF52: return;
        case 0xFF53: return;

        case 0xFF54: return;
        case 0xFF55: return;
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

        // Unimplemented: Background color palette spec/index
        case 0xFF68: return;

        // Unimplemented: Background color palette data
        case 0xFF69: return;

        // Unimplemented: OBJ color palette spec/index
        case 0xFF6A: return;

        // Unimplemented: OBJ color palette data
        case 0xFF6B: return;

        // Unimplemented: Object priority mode
        case 0xFF6C: return;

        case 0xFF6D:
        case 0xFF6E:
        case 0xFF6F: return;

        /* Unimplemented: CGB WRAM bank */
        case 0xFF70: return;

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
        valueAtAddress = read(fromAddress);
        write(toAddress, valueAtAddress);
    }
}

bool MMU::isBootROMActive() const
{
    //return read(0xFF50) != 0x1;
    return disableBootROMSwitch.getValue() == 0;
}
