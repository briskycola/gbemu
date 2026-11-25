#pragma once

#include <array>
#include <cstdint>
#include <vector>
#include "Core/Memory/IORegisters.hpp"
#include "Core/Common/Common.hpp"

// Audio is generated at this rate (samples per second, per channel)
namespace apu
{
    static constexpr uint32_t AUDIO_SAMPLE_RATE = 48000;

    // The frame sequencer runs at 512 Hz, clocked by the timer's divider
    // (see Timer), and clocks the length counters, volume envelopes, and
    // frequency sweep. It has 8 steps: lengths on even steps, sweep on
    // steps 2 and 6, and envelopes on step 7.
    constexpr uint8_t FRAME_SEQUENCER_STEP_MASK = 0x07;
    constexpr uint8_t SWEEP_STEP_A              = 2;
    constexpr uint8_t SWEEP_STEP_B              = 6;
    constexpr uint8_t ENVELOPE_STEP             = 7;
    
    // Square wave duty patterns, one bit per step:
    // 12.5%, 25%, 50%, and 75%
    constexpr uint8_t DUTY_PATTERNS[4] = {0x01, 0x81, 0x87, 0x7E};
    constexpr uint8_t DUTY_STEP_MASK = 0x07;
    constexpr uint32_t SQUARE_TIMER_PHASE_MASK = 0x03; // Low bits a trigger leaves alone
    
    // Noise channel clock divisors, indexed by NR43 bits 0-2
    constexpr uint8_t NOISE_DIVISORS[8] = {8, 16, 32, 48, 64, 80, 96, 112};
    
    // Channel periods are 11-bit values. The timer counts up
    // from the period to 2048; square channels step every 4
    // clocks and the wave channel every 2.
    constexpr uint32_t PERIOD_OVERFLOW        = 2048;
    constexpr int MAX_PERIOD                  = 2047;
    constexpr uint32_t SQUARE_CLOCKS_PER_STEP = 4;
    constexpr uint32_t WAVE_CLOCKS_PER_STEP   = 2;
    constexpr uint16_t PERIOD_HIGH_MASK       = 0x700; // Bits 8-10, written through NRx4
    constexpr uint8_t PERIOD_HIGH_BITS_MASK   = 0x07; // Bits 0-2 of NRx4
    
    // NRx4 bits shared by every channel
    constexpr uint8_t LENGTH_ENABLE_BIT = 6;
    constexpr uint8_t TRIGGER_BIT       = 7;
    
    // Length counters count down from these values. Writes
    // to NRx1 set the length to (max - value).
    constexpr uint16_t SQUARE_NOISE_MAX_LENGTH = 64;
    constexpr uint16_t WAVE_MAX_LENGTH         = 256;
    constexpr uint8_t LENGTH_MASK              = 0x3F; // Bits 0-5 of NR11/NR21/NR41
    
    // NRx2 volume envelope: bits 4-7 are the starting volume,
    // bit 3 the direction, and bits 0-2 the period. The DAC is
    // on while any of bits 3-7 is set.
    constexpr uint8_t DAC_ENABLE_MASK       = 0xF8;
    constexpr uint8_t ENVELOPE_INCREASE_BIT = 3;
    constexpr uint8_t ENVELOPE_PERIOD_MASK  = 0x07;
    constexpr uint8_t MAX_VOLUME            = 15;
    constexpr int ZOMBIE_VOLUME_FLIP        = 16; // Changing direction sets volume to 16 - volume
    
    // Envelope and sweep periods of 0 are treated as 8
    constexpr uint8_t ZERO_PERIOD_VALUE = 8;
    
    // NR10 sweep: bits 4-6 are the period, bit 3 the
    // direction (1 = subtract), and bits 0-2 the shift
    constexpr uint8_t SWEEP_PERIOD_SHIFT     = 4;
    constexpr uint8_t SWEEP_PERIOD_MASK      = 0x07;
    constexpr uint8_t SWEEP_PERIOD_BITS_MASK = 0x70;
    constexpr uint8_t SWEEP_NEGATE_BIT       = 3;
    constexpr uint8_t SWEEP_SHIFT_MASK       = 0x07;
    
    // NR30 bit 7 turns the wave channel's DAC on
    constexpr uint8_t WAVE_DAC_ENABLE_BIT = 7;
    
    // Wave RAM holds 32 4-bit samples
    constexpr uint8_t WAVE_POSITION_MASK = 0x1F;

    // After a trigger the wave channel waits this many extra clocks
    // before reading its first sample
    constexpr uint32_t WAVE_TRIGGER_DELAY = 6;

    // On the DMG the CPU can only reach wave RAM while channel 3 plays if it
    // accesses it within this many clocks of the channel reading a sample;
    // otherwise reads return 0xFF and writes are ignored. Retriggering the
    // channel this close to its next read corrupts the start of wave RAM.
    constexpr uint32_t WAVE_ACCESS_WINDOW     = 2;
    constexpr uint32_t WAVE_CORRUPTION_WINDOW = 2;
    constexpr uint8_t WAVE_CORRUPTION_BLOCK   = 4; // Bytes copied by the corruption
    
    // NR32 bits 5-6 set the wave volume: 0 = mute,
    // 1 = 100%, 2 = 50%, and 3 = 25%
    constexpr uint8_t WAVE_LEVEL_SHIFT = 5;
    constexpr uint8_t WAVE_LEVEL_MASK  = 0x03;
    
    // NR43: bits 0-2 pick the divisor, bit 3 selects the 7-bit LFSR, and
    // bits 4-7 are the clock shift; shifts of 14 and 15 stop the LFSR. The
    // LFSR has 15 bits of state and a 16th that holds the next bit in.
    // https://gbdev.io/pandocs/Audio_details.html#noise-channel-ch4
    constexpr uint8_t NOISE_DIVISOR_MASK     = 0x07;
    constexpr uint8_t NOISE_SHORT_MODE_BIT   = 3;
    constexpr uint8_t NOISE_CLOCK_SHIFT      = 4;
    constexpr uint8_t NOISE_CLOCK_SHIFT_MASK = 0x0F;
    constexpr uint8_t NOISE_STOPPED_SHIFT    = 14;
    constexpr uint8_t LFSR_NEXT_BIT          = 15;
    constexpr uint8_t LFSR_SHORT_MODE_BIT    = 7;
    
    // NRx1 bits 6-7 pick the square duty pattern
    constexpr uint8_t DUTY_SHIFT = 6;
    constexpr uint8_t DUTY_MASK  = 0x03;
    
    // NR50 master volume: bits 4-6 are the left volume
    // and bits 0-2 the right, each 0-7 meaning 1/8 to 8/8
    constexpr uint8_t LEFT_VOLUME_SHIFT  = 4;
    constexpr uint8_t MASTER_VOLUME_MASK = 0x07;
    
    // NR51 panning: bits 0-3 send channels 1-4 to the
    // right speaker, and bits 4-7 to the left
    constexpr uint8_t CHANNEL_COUNT      = 4;
    constexpr uint8_t LEFT_PANNING_SHIFT = 4;
    
    // The mixed output ranges over +-480 (4 channels of +-15 fifteenths,
    // times NR50's 1-8), scaled up to the 16-bit range
    constexpr double OUTPUT_SCALE = 64.0;

    // The high-pass filter's charge factor per clock
    // https://gbdev.io/pandocs/Audio_details.html#obscure-behavior
    constexpr double HIGH_PASS_CHARGE_DMG = 0.999958;
    constexpr double HIGH_PASS_CHARGE_CGB = 0.998943;
    
    // NR52: bit 7 is the power switch and bits 0-3 report which
    // channels are playing. Bits 4-6 are unused and read as 1.
    constexpr uint8_t POWER_BIT                 = 7;
    constexpr uint8_t SOUND_CONTROL_UNUSED_BITS = 0x70;
}

class APU
{
    private:
        // What every channel has: whether it is playing, and its length timer
        // https://gbdev.io/pandocs/Audio.html#length-timer
        class LengthChannel
        {
            public:
                bool enabled = false;
                uint16_t length = 0;
                bool lengthEnabled = false;
        };

        // A channel with a volume envelope (CH1, CH2 and CH4)
        // https://gbdev.io/pandocs/Audio.html#volume--envelope
        class EnvelopeChannel : public LengthChannel
        {
            public:
                uint8_t initialVolume = 0;
                uint8_t volume = 0;
                uint8_t envelopePeriod = 0;
                uint8_t envelopeTimer = 0;
                bool envelopeIncrease = false;
                bool envelopeRunning = false; // Stops once the volume reaches 0 or 15
        };

        class SquareChannel : public EnvelopeChannel
        {
            public:
                uint16_t frequency = 0;
                uint32_t timer = 0;
                uint8_t dutyPosition = 0;
                bool firstStepSilent = true; // Until the first duty step after power-on
        };

        class WaveChannel : public LengthChannel
        {
            public:
                uint16_t frequency = 0;
                uint32_t timer = 0;
                uint8_t position = 0;
                uint32_t cyclesSinceRead = UINT32_MAX; // Clocks since it last read wave RAM
                uint8_t sampleBuffer = 0; // The last sample read, which is what plays
        };

        class NoiseChannel : public EnvelopeChannel
        {
            public:
                uint32_t timer = 0;
                uint16_t lfsr = 0;
        };

        // Raw values of every register from NR10 to the end of wave RAM
        std::array<uint8_t, io::AUDIO_ADDRESS_END - io::AUDIO_ADDRESS_START + 1> registers{};

        // 32 4-bit samples, two per byte
        std::array<uint8_t, io::WAVE_RAM_END - io::WAVE_RAM_START + 1> waveRAM{};
        SquareChannel square1;
        SquareChannel square2;
        WaveChannel wave;
        NoiseChannel noise;
        bool powered = false;
        bool cgbMode = false; // CGB hardware: no DMG wave RAM quirks, and its own high-pass filter
        uint8_t frameSequencerStep = 0; // The next step to run
        uint8_t sweepTimer = 0;
        uint16_t sweepShadowFrequency = 0;
        bool sweepEnabled = false;
        bool sweepNegateUsed = false;
        uint32_t samplePhase = 0;
        uint32_t sampleRate = apu::AUDIO_SAMPLE_RATE;
        uint32_t clockRate = common::GAMEBOY_CLOCK_RATE; // Clocks per second of the console
        int64_t sampleLeftAccumulator = 0;
        int64_t sampleRightAccumulator = 0;
        uint32_t sampleCycleCount = 0;
        bool anyDACOn = false; // Since the last output sample
        double capacitorLeft = 0.0; // The high-pass filters' state
        double capacitorRight = 0.0;
        std::vector<int16_t> samples;

        uint8_t reg(uint16_t address) const;
        void setReg(uint16_t address, uint8_t value);
        void triggerSquare(SquareChannel &channel, bool hasSweep, uint8_t sweepReg);
        void triggerWave();
        void triggerNoise();
        void clockFrameSequencer();
        void clockLength();
        void clockEnvelopes();
        void clockSweep();
        int calculateSweep(uint8_t sweepReg);
        bool nextStepSkipsLength() const;
        void writeLengthEnable(LengthChannel &channel, uint8_t value);
        void reloadLengthOnTrigger(LengthChannel &channel, uint16_t maxLength);
        void clockOscillators();
        void accumulateSample();
        void emitSample();
        uint32_t squarePeriod(const SquareChannel &channel) const;
        uint32_t wavePeriod() const;
        uint32_t noisePeriod() const;
        int squareOutput(const SquareChannel &channel, uint8_t dutyReg) const;
        int waveOutput() const;
        int noiseOutput() const;
        void startEnvelope(EnvelopeChannel &channel, uint8_t envelope);
        void clockEnvelope(EnvelopeChannel &channel);
        void writeEnvelope(EnvelopeChannel &channel, uint8_t oldValue, uint8_t newValue);
        bool waveRAMAccessible() const;

    public:
        void setCGBMode(bool enabled) { cgbMode = enabled; }
        void setClockRate(uint32_t rate) { clockRate = rate; }
        void tick(uint32_t tCycles);

        // Called by the Timer on each falling edge of its frame sequencer bit
        void clockFrameSequencerFromDivider();
        uint8_t read(uint16_t address) const;
        void write(uint16_t address, uint8_t value);
        std::vector<int16_t> takeSamples();

        // The current digital output (0-15) of channels 1 and 2 (PCM12) or 3 and
        // 4 (PCM34): the first in the low nibble, the second in the high nibble
        // https://gbdev.io/pandocs/Audio_details.html#pcm-registers
        uint8_t readPCM12() const;
        uint8_t readPCM34() const;
};
