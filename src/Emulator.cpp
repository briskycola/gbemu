#include "Emulator.hpp"
#include "IORegisters.hpp"

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
            mmu.write(Address(BOOT_ROM_DISABLE_REGISTER), 1);
            mmu.write(Address(INTERRUPT_FLAG_REGISTER), 1 << INTERRUPT_VBLANK_BIT);

            // LCD and background on, background tiles from 0x8000
            mmu.write(Address(LCD_CONTROL_REGISTER), 0x91);

            // Background shades 3, 3, 3, 0 and both sprite palettes all black
            mmu.write(Address(BG_PALETTE_REGISTER), 0xFC);
            mmu.write(Address(OBJ_PALETTE0_REGISTER), 0xFF);
            mmu.write(Address(OBJ_PALETTE1_REGISTER), 0xFF);
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
    Cycles t{ m.cycles * T_CYCLES_PER_M_CYCLE }; // T-cycles

    // In double speed mode the CPU and timer run twice as fast,
    // but the PPU and APU keep their normal speed
    elapsedCycles += t.cycles;
    ppu.tick(Cycles(doubleSpeed ? t.cycles / 2 : t.cycles));
    timer.tick(t.cycles);
    apu.tick(doubleSpeed ? t.cycles / 2 : t.cycles);
}

void Emulator::runFrame(const std::function<void(const FrameBuffer&)> &vblankCallback)
{
    bool frameDrawn = false;
    ppu.registerVBlankCallback([&](const FrameBuffer &buffer)
    {
        vblankCallback(buffer);
        frameDrawn = true;
    });

    // When the LCD is off the PPU never reaches VBlank,
    // so cap the loop at one frame's worth of cycles
    // (twice as many CPU cycles in double speed mode).
    const uint32_t startCycles = elapsedCycles;
    const uint32_t frameBudget = CLOCKS_PER_FRAME * (doubleSpeed ? 2 : 1);

    while (!frameDrawn && elapsedCycles - startCycles < frameBudget)
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
