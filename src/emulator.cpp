#include "emulator.hpp"

Emulator::Emulator(const std::vector<uint8_t> &cartridgeData, const std::vector<uint8_t> &saveData) :
    cartridge(getCartridge(cartridgeData, saveData)),
    cpu(*this),
    mmu(*this),
    timer(*this),
    serial()
{}

const std::vector<uint8_t>& Emulator::getCartridgeRAM()
{
    return cartridge->getCartridgeRAM();
}

void Emulator::tick()
{
    Cycles cycles = cpu.tick();
    elapsedCycles += cycles.cycles;
    timer.tick(cycles.cycles);
}

void Emulator::run()
{
    // Right now, I'm just going to make
    // sure NOP runs infinitely
    while (true)
    {
        tick();
    }
}

void Emulator::buttonPressed(GBButton button)
{
    input.buttonPressed(button);
}

void Emulator::buttonReleased(GBButton button)
{
    input.buttonReleased(button);
}
