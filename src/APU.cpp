#include "apu.hpp"
#include <algorithm>

namespace
{
constexpr uint32_t kClockRate = 4194304;
constexpr uint32_t kFrameSequencerPeriod = 8192;
constexpr uint8_t kDutyPatterns[4] = {0x01, 0x81, 0x87, 0x7E};
constexpr uint8_t kNoiseDivisors[8] = {8, 16, 32, 48, 64, 80, 96, 112};
}

uint8_t APU::reg(uint16_t address) const
{
    return registers[address - 0xFF10];
}

void APU::setReg(uint16_t address, uint8_t value)
{
    registers[address - 0xFF10] = value;
}

uint32_t APU::squarePeriod(const SquareChannel &channel) const
{
    return std::max<uint32_t>(4, (2048u - channel.frequency) * 4u);
}

uint32_t APU::wavePeriod() const
{
    return std::max<uint32_t>(2, (2048u - wave.frequency) * 2u);
}

uint32_t APU::noisePeriod() const
{
    return static_cast<uint32_t>(kNoiseDivisors[reg(0xFF22) & 0x07])
        << ((reg(0xFF22) >> 4) & 0x0F);
}

void APU::disableIfDacOff(SquareChannel &channel, uint8_t envelopeReg)
{
    if ((envelopeReg & 0xF8) == 0) channel.enabled = false;
}

void APU::disableIfDacOff(NoiseChannel &channel, uint8_t envelopeReg)
{
    if ((envelopeReg & 0xF8) == 0) channel.enabled = false;
}

void APU::triggerSquare(SquareChannel &channel, bool hasSweep, uint8_t sweepReg)
{
    if (channel.length == 0) channel.length = 64;
    channel.enabled = (reg(&channel == &square1 ? 0xFF12 : 0xFF17) & 0xF8) != 0;
    channel.timer = squarePeriod(channel);
    channel.dutyPosition = 0;
    const uint8_t envelope = reg(&channel == &square1 ? 0xFF12 : 0xFF17);
    channel.initialVolume = envelope >> 4;
    channel.volume = channel.initialVolume;
    channel.envelopeIncrease = (envelope & 0x08) != 0;
    channel.envelopePeriod = envelope & 0x07;
    channel.envelopeTimer = channel.envelopePeriod ? channel.envelopePeriod : 8;

    if (hasSweep)
    {
        sweepShadowFrequency = channel.frequency;
        sweepTimer = (sweepReg >> 4) & 0x07;
        if (sweepTimer == 0) sweepTimer = 8;
        sweepEnabled = ((sweepReg & 0x70) != 0) || ((sweepReg & 0x07) != 0);
        sweepNegateUsed = false;
        if ((sweepReg & 0x07) != 0)
        {
            const uint16_t delta = sweepShadowFrequency >> (sweepReg & 0x07);
            const int calculated = (sweepReg & 0x08)
                ? static_cast<int>(sweepShadowFrequency) - delta
                : static_cast<int>(sweepShadowFrequency) + delta;
            if (calculated > 2047) channel.enabled = false;
            if (sweepReg & 0x08) sweepNegateUsed = true;
        }
    }
}

void APU::triggerWave()
{
    if (wave.length == 0) wave.length = 256;
    wave.enabled = (reg(0xFF1A) & 0x80) != 0;
    wave.timer = wavePeriod();
    wave.position = 0;
}

void APU::triggerNoise()
{
    if (noise.length == 0) noise.length = 64;
    noise.enabled = (reg(0xFF21) & 0xF8) != 0;
    noise.timer = noisePeriod();
    noise.lfsr = 0x7FFF;
    const uint8_t envelope = reg(0xFF21);
    noise.initialVolume = envelope >> 4;
    noise.volume = noise.initialVolume;
    noise.envelopeIncrease = (envelope & 0x08) != 0;
    noise.envelopePeriod = envelope & 0x07;
    noise.envelopeTimer = noise.envelopePeriod ? noise.envelopePeriod : 8;
}

void APU::clockLength()
{
    auto clockSquare = [](SquareChannel &channel) {
        if (channel.lengthEnabled && channel.length > 0 && --channel.length == 0)
            channel.enabled = false;
    };
    clockSquare(square1);
    clockSquare(square2);
    if (wave.lengthEnabled && wave.length > 0 && --wave.length == 0) wave.enabled = false;
    if (noise.lengthEnabled && noise.length > 0 && --noise.length == 0) noise.enabled = false;
}

void APU::clockEnvelopes()
{
    auto clockSquare = [](SquareChannel &channel) {
        if (channel.envelopePeriod == 0) return;
        if (channel.envelopeTimer > 0) --channel.envelopeTimer;
        if (channel.envelopeTimer == 0)
        {
            channel.envelopeTimer = channel.envelopePeriod;
            if (channel.envelopeIncrease && channel.volume < 15) ++channel.volume;
            else if (!channel.envelopeIncrease && channel.volume > 0) --channel.volume;
        }
    };
    clockSquare(square1);
    clockSquare(square2);

    if (noise.envelopePeriod == 0) return;
    if (noise.envelopeTimer > 0) --noise.envelopeTimer;
    if (noise.envelopeTimer == 0)
    {
        noise.envelopeTimer = noise.envelopePeriod;
        if (noise.envelopeIncrease && noise.volume < 15) ++noise.volume;
        else if (!noise.envelopeIncrease && noise.volume > 0) --noise.volume;
    }
}

void APU::clockSweep()
{
    if (sweepTimer > 0) --sweepTimer;
    if (sweepTimer != 0) return;

    const uint8_t sweep = reg(0xFF10);
    sweepTimer = (sweep >> 4) & 0x07;
    if (sweepTimer == 0) sweepTimer = 8;
    const uint8_t shift = sweep & 0x07;
    if (!sweepEnabled || shift == 0) return;

    const uint16_t delta = sweepShadowFrequency >> shift;
    const int calculated = (sweep & 0x08)
        ? static_cast<int>(sweepShadowFrequency) - delta
        : static_cast<int>(sweepShadowFrequency) + delta;
    if (sweep & 0x08) sweepNegateUsed = true;
    if (calculated > 2047 || calculated < 0)
    {
        square1.enabled = false;
        return;
    }

    sweepShadowFrequency = static_cast<uint16_t>(calculated);
    square1.frequency = sweepShadowFrequency;
    setReg(0xFF13, static_cast<uint8_t>(calculated));
    setReg(0xFF14, static_cast<uint8_t>((reg(0xFF14) & 0xF8) | ((calculated >> 8) & 0x07)));

    const uint16_t nextDelta = sweepShadowFrequency >> shift;
    const int next = (sweep & 0x08)
        ? static_cast<int>(sweepShadowFrequency) - nextDelta
        : static_cast<int>(sweepShadowFrequency) + nextDelta;
    if (next > 2047 || next < 0) square1.enabled = false;
}

void APU::clockFrameSequencer()
{
    if ((frameSequencerStep & 1) == 0) clockLength();
    if (frameSequencerStep == 2 || frameSequencerStep == 6) clockSweep();
    if (frameSequencerStep == 7) clockEnvelopes();
    frameSequencerStep = (frameSequencerStep + 1) & 0x07;
}

void APU::clockOscillators()
{
    auto clockSquare = [this](SquareChannel &channel) {
        if (!channel.enabled) return;
        if (channel.timer > 0) --channel.timer;
        if (channel.timer == 0)
        {
            channel.timer = squarePeriod(channel);
            channel.dutyPosition = (channel.dutyPosition + 1) & 0x07;
        }
    };
    clockSquare(square1);
    clockSquare(square2);

    if (wave.enabled)
    {
        if (wave.timer > 0) --wave.timer;
        if (wave.timer == 0)
        {
            wave.timer = wavePeriod();
            wave.position = (wave.position + 1) & 0x1F;
        }
    }

    if (noise.enabled)
    {
        if (noise.timer > 0) --noise.timer;
        if (noise.timer == 0)
        {
            noise.timer = noisePeriod();
            const uint16_t bit = (noise.lfsr ^ (noise.lfsr >> 1)) & 1;
            noise.lfsr = (noise.lfsr >> 1) | (bit << 14);
            if (reg(0xFF22) & 0x08)
                noise.lfsr = (noise.lfsr & ~(1u << 6)) | (bit << 6);
        }
    }
}

int APU::squareOutput(const SquareChannel &channel, uint8_t dutyReg) const
{
    if (!channel.enabled) return 0;
    const uint8_t duty = (dutyReg >> 6) & 0x03;
    const int bit = (kDutyPatterns[duty] >> channel.dutyPosition) & 1;
    return bit ? channel.volume : 0;
}

int APU::waveOutput() const
{
    if (!wave.enabled) return 0;
    const uint8_t level = (reg(0xFF1C) >> 5) & 0x03;
    if (level == 0) return 0;
    const uint8_t packed = waveRAM[wave.position >> 1];
    const uint8_t sample = (wave.position & 1) ? (packed & 0x0F) : (packed >> 4);
    return sample >> (level - 1);
}

int APU::noiseOutput() const
{
    if (!noise.enabled || (noise.lfsr & 1)) return 0;
    return noise.volume;
}

void APU::accumulateSample()
{
    const int outputs[4] = {
        squareOutput(square1, reg(0xFF11)),
        squareOutput(square2, reg(0xFF16)),
        waveOutput(),
        noiseOutput()
    };
    const bool active[4] = {square1.enabled, square2.enabled, wave.enabled, noise.enabled};
    const uint8_t routing = reg(0xFF25);
    int left = 0;
    int right = 0;
    int leftChannels = 0;
    int rightChannels = 0;
    for (int channel = 0; channel < 4; ++channel)
    {
        const int output = active[channel] ? outputs[channel] * 2 - 15 : 0;
        if ((routing & (1 << channel)) && active[channel])
        {
            right += output;
            ++rightChannels;
        }
        if ((routing & (1 << (channel + 4))) && active[channel])
        {
            left += output;
            ++leftChannels;
        }
    }
    if (!powered || leftChannels == 0) left = 0;
    if (!powered || rightChannels == 0) right = 0;
    sampleLeftAccumulator += left;
    sampleRightAccumulator += right;
    ++sampleCycleCount;
}

void APU::emitSample()
{
    int left = sampleCycleCount ? static_cast<int>(sampleLeftAccumulator / sampleCycleCount) : 0;
    int right = sampleCycleCount ? static_cast<int>(sampleRightAccumulator / sampleCycleCount) : 0;
    const uint8_t volume = reg(0xFF24);
    left *= (((volume >> 4) & 0x07) + 1) * 64;
    right *= ((volume & 0x07) + 1) * 64;

    // Approximate the DMG output coupling capacitors, which remove DC bias
    // and soften the large offsets produced by pulse-wave duty cycles.
    constexpr int32_t highPassCoefficient = 65274; // 0.996 in Q16
    const int32_t filteredLeft = left - previousInputLeft
        + static_cast<int32_t>((static_cast<int64_t>(previousOutputLeft) * highPassCoefficient) >> 16);
    const int32_t filteredRight = right - previousInputRight
        + static_cast<int32_t>((static_cast<int64_t>(previousOutputRight) * highPassCoefficient) >> 16);
    previousInputLeft = left;
    previousInputRight = right;
    previousOutputLeft = filteredLeft;
    previousOutputRight = filteredRight;
    left = filteredLeft;
    right = filteredRight;

    left = std::max(-32768, std::min(32767, left));
    right = std::max(-32768, std::min(32767, right));
    samples.push_back(static_cast<int16_t>(left));
    samples.push_back(static_cast<int16_t>(right));
    sampleLeftAccumulator = 0;
    sampleRightAccumulator = 0;
    sampleCycleCount = 0;
}

void APU::tick(uint32_t tCycles)
{
    for (uint32_t cycle = 0; cycle < tCycles; ++cycle)
    {
        if (powered)
        {
            clockOscillators();
            if (++frameSequencerCycles >= kFrameSequencerPeriod)
            {
                frameSequencerCycles = 0;
                clockFrameSequencer();
            }
        }

        accumulateSample();
        samplePhase += sampleRate;
        if (samplePhase >= kClockRate)
        {
            samplePhase -= kClockRate;
            emitSample();
        }
    }
}

uint8_t APU::read(uint16_t address) const
{
    if (address >= 0xFF30)
    {
        const uint8_t index = wave.enabled ? (wave.position >> 1) : (address - 0xFF30);
        return waveRAM[index];
    }
    if (address == 0xFF26)
    {
        return static_cast<uint8_t>(0x70 | (powered ? 0x80 : 0)
            | (square1.enabled ? 1 : 0) | (square2.enabled ? 2 : 0)
            | (wave.enabled ? 4 : 0) | (noise.enabled ? 8 : 0));
    }

    static constexpr uint8_t readMasks[0x16] = {
        0x80, 0x3F, 0x00, 0xFF, 0xBF, 0xFF, 0x3F, 0x00,
        0xFF, 0xBF, 0x7F, 0xFF, 0x9F, 0xFF, 0xBF, 0xFF,
        0x3F, 0x00, 0x00, 0xBF, 0x00, 0x00
    };
    return reg(address) | readMasks[address - 0xFF10];
}

void APU::write(uint16_t address, uint8_t value)
{
    if (address >= 0xFF30)
    {
        const uint8_t index = wave.enabled ? (wave.position >> 1) : (address - 0xFF30);
        waveRAM[index] = value;
        return;
    }
    if (address == 0xFF26)
    {
        const bool newPower = (value & 0x80) != 0;
        if (powered && !newPower)
        {
            powered = false;
            registers.fill(0);
            square1 = SquareChannel{};
            square2 = SquareChannel{};
            wave = WaveChannel{};
            noise = NoiseChannel{};
            frameSequencerCycles = 0;
            frameSequencerStep = 0;
            sweepTimer = 0;
            sweepShadowFrequency = 0;
            sweepEnabled = false;
            sweepNegateUsed = false;
        }
        else if (!powered && newPower)
        {
            powered = true;
            frameSequencerCycles = 0;
            frameSequencerStep = 0;
        }
        return;
    }

    if (address < 0xFF10 || address > 0xFF25) return;
    if (!powered)
    {
        if (address == 0xFF11) square1.length = 64 - (value & 0x3F);
        else if (address == 0xFF16) square2.length = 64 - (value & 0x3F);
        else if (address == 0xFF1B) wave.length = 256 - value;
        else if (address == 0xFF20) noise.length = 64 - (value & 0x3F);
        return;
    }

    const uint8_t oldValue = reg(address);
    setReg(address, value);
    switch (address)
    {
        case 0xFF10:
            if (sweepNegateUsed && (oldValue & 0x08) && !(value & 0x08)) square1.enabled = false;
            break;
        case 0xFF11: square1.length = 64 - (value & 0x3F); break;
        case 0xFF12: disableIfDacOff(square1, value); break;
        case 0xFF13: square1.frequency = (square1.frequency & 0x700) | value; break;
        case 0xFF14:
            square1.frequency = (square1.frequency & 0xFF) | ((value & 0x07) << 8);
            square1.lengthEnabled = (value & 0x40) != 0;
            if (value & 0x80) triggerSquare(square1, true, reg(0xFF10));
            break;
        case 0xFF16: square2.length = 64 - (value & 0x3F); break;
        case 0xFF17: disableIfDacOff(square2, value); break;
        case 0xFF18: square2.frequency = (square2.frequency & 0x700) | value; break;
        case 0xFF19:
            square2.frequency = (square2.frequency & 0xFF) | ((value & 0x07) << 8);
            square2.lengthEnabled = (value & 0x40) != 0;
            if (value & 0x80) triggerSquare(square2, false, 0);
            break;
        case 0xFF1A: if (!(value & 0x80)) wave.enabled = false; break;
        case 0xFF1B: wave.length = 256 - value; break;
        case 0xFF1C: break;
        case 0xFF1D: wave.frequency = (wave.frequency & 0x700) | value; break;
        case 0xFF1E:
            wave.frequency = (wave.frequency & 0xFF) | ((value & 0x07) << 8);
            wave.lengthEnabled = (value & 0x40) != 0;
            if (value & 0x80) triggerWave();
            break;
        case 0xFF20: noise.length = 64 - (value & 0x3F); break;
        case 0xFF21: disableIfDacOff(noise, value); break;
        case 0xFF22: break;
        case 0xFF23:
            noise.lengthEnabled = (value & 0x40) != 0;
            if (value & 0x80) triggerNoise();
            break;
        default: break;
    }
}

std::vector<int16_t> APU::takeSamples()
{
    std::vector<int16_t> result;
    result.swap(samples);
    return result;
}
