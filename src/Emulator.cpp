#include "Emulator.hpp"
#include "IORegisters.hpp"
#include "CartridgeMetadata.hpp"
#include <stdexcept>
#include <string>

bool Emulator::usesCGBBootROM(const std::vector<uint8_t> &cartridgeData, HardwareModel model)
{
    return model == HardwareModel::GameBoyColor ||
        (model == HardwareModel::Automatic && getInfo(cartridgeData)->supportsCGB);
}

Emulator::Emulator(const std::vector<uint8_t> &cartridgeData, const std::vector<uint8_t> &saveData,
                   const std::vector<uint8_t> &bootROM, HardwareModel model) :
    cartridge(getCartridge(cartridgeData, saveData)),
    cgbHardware(usesCGBBootROM(cartridgeData, model)),
    // The boot ROM runs in CGB Mode. As it hands over it sets KEY0, which
    // leaves CGB Mode on for Game Boy Color games and turns it off for the
    // rest (MMU, BANK writes).
    cgbMode(cgbHardware),
    sgbMode(!cgbHardware && cartridge->supportsSGB()),
    cpu(*this),
    mmu(*this),
    timer(*this),
    serial(*this),
    input(*this),
    ppu(*this),
    apu()
{
    // Every start runs a real boot ROM from address 0, which sets up the
    // hardware and then hands control to the cartridge at $0100
    const size_t expectedSize = cgbHardware ? mmu::CGB_BOOT_ROM_SIZE : mmu::DMG_BOOT_ROM_SIZE;
    if (bootROM.size() != expectedSize)
    {
        throw std::runtime_error(std::string(cgbHardware ? "The Game Boy Color" : "The Game Boy")
            + " boot ROM must be exactly " + std::to_string(expectedSize) + " bytes, but it is "
            + std::to_string(bootROM.size()) + " bytes");
    }
    mmu.loadBootROM(bootROM);
    apu.setCGBMode(cgbHardware);
}

std::vector<uint8_t> Emulator::getSaveData() const
{
    return cartridge->saveData();
}

std::vector<int16_t> Emulator::takeAudioSamples()
{
    return apu.takeSamples();
}

void Emulator::tick()
{
    // Bus accesses already advanced the system by one machine cycle each
    // while the instruction ran; advance the rest (internal cycles) now.
    const Cycles m = cpu.tick();

    // In STOP mode the system clock is halted, so nothing else runs. Time
    // still passes for the frame budget so the frontend stays responsive.
    // https://gbdev.io/pandocs/Reducing_Power_Consumption.html#using-the-stop-instruction
    if (cpu.stopped())
    {
        elapsedCycles += m.cycles * common::T_CYCLES_PER_M_CYCLE;
        return;
    }

    const uint32_t alreadyAdvanced = cpu.takeBusCycles();
    if (m.cycles > alreadyAdvanced) advance(m.cycles - alreadyAdvanced);
}

void Emulator::advance(const uint32_t mCycles)
{
    const Cycles m{ mCycles };
    const Cycles t{ m.cycles * common::T_CYCLES_PER_M_CYCLE }; // T-cycles

    elapsedCycles += t.cycles;

    // The cartridge's own hardware, like the MBC3 clock, runs in real time,
    // so it sees half as many clocks per CPU cycle in double speed
    cartridge->tick(doubleSpeed ? t.cycles / 2 : t.cycles);

    // OAM DMA runs at CPU speed, one byte per machine cycle
    mmu.tickOAMDMA(m.cycles);

    // In double speed mode the CPU and timer run twice as fast,
    // but the PPU and APU keep their normal speed.
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
    const uint32_t frameBudget = ppu::CLOCKS_PER_FRAME * (doubleSpeed ? 2 : 1);

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
