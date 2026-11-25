#pragma once

#include "CPU.hpp"
#include "Cartridge.hpp"
#include "MMU.hpp"
#include "Timer.hpp"
#include "Serial.hpp"
#include "Input.hpp"
#include "PPU.hpp"
#include "APU.hpp"
#include "SGB.hpp"
#include <memory>

// Which console to emulate. Automatic runs Game Boy Color games as a Game Boy
// Color and everything else as a Game Boy; GameBoy always emulates the
// original Game Boy (DMG), even for games that support the Game Boy Color;
// GameBoyColor always emulates the Game Boy Color, which runs Game Boy games
// in its DMG compatibility mode.
// https://gbdev.io/pandocs/CGB_Registers.html#ff4c--key0sys-cgb-mode-only-cpu-mode-select
enum class HardwareModel
{
    Automatic,
    GameBoy,
    GameBoyColor,
};

class Emulator
{
    private:
        std::shared_ptr<Cartridge> cartridge;
        uint32_t elapsedCycles = 0;
        // VRAM DMA (both general-purpose and HBlank blocks) stalls the CPU for
        // this many machine cycles; the PPU and everything else keep running
        // https://gbdev.io/pandocs/VDMA.html#transfer-timings
        uint16_t dmaCyclesRemaining = 0;
        // Running on Game Boy Color hardware, and whether its CGB features are
        // unlocked (CGB Mode). A Game Boy game on the Game Boy Color runs in
        // DMG compatibility mode, with cgbHardware set and cgbMode clear.
        // https://gbdev.io/pandocs/CGB_Registers.html#unlocking-cgb-functions
        bool cgbHardware = false;
        bool cgbMode = false;
        bool doubleSpeed = false;
        bool speedSwitchPrepared = false;
        uint32_t speedSwitchPause = 0; // Machine cycles the CPU still waits after a speed switch
        bool sgbMode = false;
        SGB sgb;

        friend class CPU;
        CPU cpu;

        friend class MMU;
        MMU mmu;

        friend class Timer;
        Timer timer;

        friend class Serial;
        Serial serial;

        friend class Input;
        Input input;

        friend class PPU;
        PPU ppu;

        friend class APU;
        APU apu;

        void tick();

        // Runs everything but the CPU (DMA, PPU, timer, APU) for some machine cycles
        void advance(uint32_t mCycles);
    public:
        // Runs the given boot ROM first: the Game Boy Color one (2304 bytes) when
        // usesCGBBootROM() is true for this ROM and model, otherwise the Game Boy
        // one (256 bytes). Throws if the boot ROM is missing or the wrong size.
        Emulator(const std::vector<uint8_t> &cartridgeData, const std::vector<uint8_t> &saveData,
                 const std::vector<uint8_t> &bootROM, HardwareModel model);

        // Whether a ROM runs in Game Boy Color mode, and so needs the
        // Game Boy Color boot ROM, on the given model
        static bool usesCGBBootROM(const std::vector<uint8_t> &cartridgeData, HardwareModel model);

        // What the cartridge's battery keeps (RAM, and the MBC3 clock); empty
        // for cartridges without a battery
        std::vector<uint8_t> getSaveData() const;
        std::vector<int16_t> takeAudioSamples();
        void runFrame(const std::function<void(const FrameBuffer&)> &vblankCallback);

        void buttonPressed(GBButton button);
        void buttonReleased(GBButton button);
};
