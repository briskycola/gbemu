#pragma once

#include <array>
#include <cstdint>
#include <cstddef>
#include <vector>
#include "IORegisters.hpp"

// Audio is generated at this rate (samples per second, per channel)
static constexpr uint32_t AUDIO_SAMPLE_RATE = 48000;

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
