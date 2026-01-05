#include "mmu.hpp"
#include "address.hpp"
#include "emulator.hpp"
#include "cpu.hpp"
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
        return emulator.cartridge->read(address);
    }

    // VRAM
    if (address.inRange(VRAM_ADDRESS_START, VRAM_ADDRESS_END))
    {
        // TODO
        return 0xFF;
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
        std::cerr << "Reading from unusable memory: " << std::hex << address.getValue();
        return 0xFF;
    }

    // I/O Registers
    if (address.inRange(IO_ADDRESS_START, IO_ADDRESS_END))
    {
        // TODO
        return 0xFF;
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
        // TODO
        return;
    }

    // Cartridge RAM (external)
    if (address.inRange(CARTRIDGE_RAM_ADDRESS_START, CARTRIDGE_RAM_ADDRESS_END))
    {
        emulator.cartridge->write(address, value);
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
        std::cerr << "Writing to unusable memory: " << std::hex << address.getValue();
        return;
    }

    // I/O Registers
    if (address.inRange(IO_ADDRESS_START, IO_ADDRESS_END))
    {
        // TODO
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

void MMU::dmaTransfer(const uint8_t value)
{
    uint8_t valueAtAddress;
    Address startAddress = value * DMA_SOURCE_PAGE_SHIFT;

    for (uint8_t i = 0; i <= DMA_TRANSFER_LENGTH; i++)
    {
        Address fromAddress = startAddress.getValue() + i;
        Address toAddress = OAM_OFFSET + i;
        valueAtAddress = read(fromAddress);
        write(toAddress, valueAtAddress);
    }
}
