#include "emulator.hpp"

Emulator::Emulator(const std::vector<uint8_t> &cartridgeData, const std::vector<uint8_t> &saveData) :
    cartridge(getCartridge(cartridgeData, saveData)),
    cpu(*this),
    mmu(*this),
    timer(*this),
    serial(),
    ppu(*this),
    apu()
{}

const std::vector<uint8_t>& Emulator::getCartridgeRAM()
{
    return cartridge->getCartridgeRAM();
}

std::vector<int16_t> Emulator::takeAudioSamples()
{
    return apu.takeSamples();
}

void Emulator::tick()
{
    Cycles m = cpu.tick();    // M-cycles
    Cycles t{ m.cycles * 4 }; // T-cycles

    elapsedCycles += t.cycles;
    ppu.tick(t);
    timer.tick(t.cycles);
    apu.tick(t.cycles);
}

void Emulator::run(std::function<bool()> shoudCloseCallback, std::function<void(const FrameBuffer&)> vblankCallback)
{
    this->shouldCloseCallback = shoudCloseCallback;
    ppu.registerVBlankCallback(vblankCallback);

    while (!shoudCloseCallback())
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
