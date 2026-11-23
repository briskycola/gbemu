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
// original Game Boy (DMG), even for games that support the Game Boy Color.
enum class HardwareModel
{
    Automatic,
    GameBoy,
};

class Emulator
{
    private:
        std::shared_ptr<Cartridge> cartridge;
        uint32_t elapsedCycles = 0;
        uint16_t dmaCyclesRemaining = 0;
        // General-purpose DMA (GDMA) freezes both the CPU and the PPU for
        // the entire transfer duration.  HDMA blocks freeze only the CPU
        // (through dmaCyclesRemaining); OAM DMA freezes neither.
        // We track the two stalls separately so tick() can suppress PPU
        // advancement during a GDMA.
        uint32_t gdmaCyclesRemaining = 0;
        bool cgbMode = false;
        bool doubleSpeed = false;
        bool speedSwitchPrepared = false;
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

        const std::vector<uint8_t>& getCartridgeRAM();
        std::vector<int16_t> takeAudioSamples();
        void runFrame(const std::function<void(const FrameBuffer&)> &vblankCallback);

        void buttonPressed(GBButton button);
        void buttonReleased(GBButton button);
};
