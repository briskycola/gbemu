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

class Emulator
{
    private:
        std::shared_ptr<Cartridge> cartridge;
        uint32_t elapsedCycles = 0;
        uint16_t dmaCyclesRemaining = 0;
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

        friend class MMU;
        APU apu;

        void tick();
    public:
        Emulator(const std::vector<uint8_t> &cartridgeData, const std::vector<uint8_t> &saveData = {},
                 const std::vector<uint8_t> &cgbBootROM = {});
        const std::vector<uint8_t>& getCartridgeRAM();
        std::vector<int16_t> takeAudioSamples();
        void runFrame(const std::function<void(const FrameBuffer&)> &vblankCallback);

        void buttonPressed(GBButton button);
        void buttonReleased(GBButton button);
};
