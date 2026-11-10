#pragma once

#include <array>
#include <cstdint>
#include <cstddef>
#include <vector>
#include "IORegisters.hpp"

// Audio is generated at this rate (samples per second, per channel)
namespace apu
{
    static constexpr uint32_t AUDIO_SAMPLE_RATE = 48000;

    // The frame sequencer runs at 512 Hz and clocks the length
    // counters, volume envelopes, and frequency sweep. It has
    // 8 steps: lengths on even steps, sweep on steps 2 and 6,
    // and envelopes on step 7.
    constexpr uint32_t FRAME_SEQUENCER_PERIOD   = 8192;
    constexpr uint8_t FRAME_SEQUENCER_STEP_MASK = 0x07;
    constexpr uint8_t SWEEP_STEP_A              = 2;
    constexpr uint8_t SWEEP_STEP_B              = 6;
    constexpr uint8_t ENVELOPE_STEP             = 7;
    
    // Square wave duty patterns, one bit per step:
    // 12.5%, 25%, 50%, and 75%
    constexpr uint8_t DUTY_PATTERNS[4] = {0x01, 0x81, 0x87, 0x7E};
    constexpr uint8_t DUTY_STEP_MASK = 0x07;
    
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
    
    // NR32 bits 5-6 set the wave volume: 0 = mute,
    // 1 = 100%, 2 = 50%, and 3 = 25%
    constexpr uint8_t WAVE_LEVEL_SHIFT = 5;
    constexpr uint8_t WAVE_LEVEL_MASK  = 0x03;
    
    // NR43: bits 0-2 pick the divisor, bit 3 selects the 7-bit
    // LFSR, and bits 4-7 are the clock shift. The LFSR is 15 bits
    // wide and resets to all ones.
    constexpr uint8_t NOISE_DIVISOR_MASK     = 0x07;
    constexpr uint8_t NOISE_SHORT_MODE_BIT   = 3;
    constexpr uint8_t NOISE_CLOCK_SHIFT      = 4;
    constexpr uint8_t NOISE_CLOCK_SHIFT_MASK = 0x0F;
    constexpr uint16_t LFSR_RESET_VALUE      = 0x7FFF;
    constexpr uint8_t LFSR_HIGH_BIT          = 14;
    constexpr uint8_t LFSR_SHORT_MODE_BIT    = 6;
    
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
    
    // Each channel outputs 0-15. Doubling and subtracting 15 centers it
    // around 0, and the summed result is scaled up to the 16-bit range.
    constexpr int CHANNEL_OUTPUT_CENTER = 15;
    constexpr int OUTPUT_SCALE          = 64;
    
    // NR52: bit 7 is the power switch and bits 0-3 report which
    // channels are playing. Bits 4-6 are unused and read as 1.
    constexpr uint8_t POWER_BIT                 = 7;
    constexpr uint8_t SOUND_CONTROL_UNUSED_BITS = 0x70;
}

class APU
{
    struct SquareChannel
    {
        bool enabled = false;
        uint16_t frequency = 0;
        uint32_t timer = 0;
        uint8_t dutyPosition = 0;
        uint16_t length = 0;
        bool lengthEnabled = false;
        uint8_t initialVolume = 0;
        uint8_t volume = 0;
        uint8_t envelopePeriod = 0;
        uint8_t envelopeTimer = 0;
        bool envelopeIncrease = false;
    };

    struct WaveChannel
    {
        bool enabled = false;
        uint16_t frequency = 0;
        uint32_t timer = 0;
        uint8_t position = 0;
        uint16_t length = 0;
        bool lengthEnabled = false;
    };

    struct NoiseChannel
    {
        bool enabled = false;
        uint32_t timer = 0;
        uint16_t lfsr = 0x7FFF; // All 15 bits set
        uint16_t length = 0;
        bool lengthEnabled = false;
        uint8_t initialVolume = 0;
        uint8_t volume = 0;
        uint8_t envelopePeriod = 0;
        uint8_t envelopeTimer = 0;
        bool envelopeIncrease = false;
    };

    // Raw values of every register from NR10 to the end of wave RAM
    std::array<uint8_t, AUDIO_ADDRESS_END - AUDIO_ADDRESS_START + 1> registers{};

    // 32 4-bit samples, two per byte
    std::array<uint8_t, WAVE_RAM_END - WAVE_RAM_START + 1> waveRAM{};
    SquareChannel square1;
    SquareChannel square2;
    WaveChannel wave;
    NoiseChannel noise;
    bool powered = false;
    uint32_t frameSequencerCycles = 0;
    uint8_t frameSequencerStep = 0;
    uint8_t sweepTimer = 0;
    uint16_t sweepShadowFrequency = 0;
    bool sweepEnabled = false;
    bool sweepNegateUsed = false;
    uint32_t samplePhase = 0;
    uint32_t sampleRate = AUDIO_SAMPLE_RATE;
    int64_t sampleLeftAccumulator = 0;
    int64_t sampleRightAccumulator = 0;
    uint32_t sampleCycleCount = 0;
    int32_t previousInputLeft = 0;
    int32_t previousInputRight = 0;
    int32_t previousOutputLeft = 0;
    int32_t previousOutputRight = 0;
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
    void clockOscillators();
    void accumulateSample();
    void emitSample();
    uint32_t squarePeriod(const SquareChannel &channel) const;
    uint32_t wavePeriod() const;
    uint32_t noisePeriod() const;
    int squareOutput(const SquareChannel &channel, uint8_t dutyReg) const;
    int waveOutput() const;
    int noiseOutput() const;
    void disableIfDacOff(SquareChannel &channel, uint8_t envelopeReg);
    void disableIfDacOff(NoiseChannel &channel, uint8_t envelopeReg);

public:
    void tick(uint32_t tCycles);
    uint8_t read(uint16_t address) const;
    void write(uint16_t address, uint8_t value);
    std::vector<int16_t> takeSamples();
};
