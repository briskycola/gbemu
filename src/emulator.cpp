#include "emulator.hpp"

Emulator::Emulator(const std::vector<uint8_t> &cartridgeData, const std::vector<uint8_t> &saveData,
                   const std::vector<uint8_t> &cgbBootROM) :
    cartridge(getCartridge(cartridgeData, saveData)),
    cgbMode(cartridge->supportsCGB()),
    sgbMode(!cgbMode && cartridge->supportsSGB()),
    cpu(*this),
    mmu(*this),
    timer(*this),
    serial(),
    ppu(*this),
    apu()
{
    const bool cgbBootLoaded = !cgbBootROM.empty() && mmu.loadCGBBootROM(cgbBootROM);
    if (cgbMode)
    {
        if (!cgbBootLoaded)
        {
            // Without a BIOS image, start from the documented CGB
            // post-boot state rather than running the DMG boot ROM.
            cpu.initializeCGBPostBoot();
            mmu.write(Address(0xFF50), 1);
            mmu.write(Address(0xFF0F), 1);
            mmu.write(Address(0xFF40), 0x91);
            mmu.write(Address(0xFF47), 0xFC);
            mmu.write(Address(0xFF48), 0xFF);
            mmu.write(Address(0xFF49), 0xFF);
        }
    }
}

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
    ppu.tick(Cycles(doubleSpeed ? t.cycles / 2 : t.cycles));
    timer.tick(t.cycles);
    apu.tick(doubleSpeed ? t.cycles / 2 : t.cycles);
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
