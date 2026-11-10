#include "APU.hpp"
#include "Common.hpp"
#include <algorithm>
#include <limits>

// https://gbdev.io/pandocs/Audio_Registers.html

uint8_t APU::reg(uint16_t address) const
{
    return registers[address - io::AUDIO_ADDRESS_START];
}

void APU::setReg(uint16_t address, uint8_t value)
{
    registers[address - io::AUDIO_ADDRESS_START] = value;
}

uint32_t APU::squarePeriod(const SquareChannel &channel) const
{
    return std::max<uint32_t>(apu::SQUARE_CLOCKS_PER_STEP, (apu::PERIOD_OVERFLOW - channel.frequency) * apu::SQUARE_CLOCKS_PER_STEP);
}

uint32_t APU::wavePeriod() const
{
    return std::max<uint32_t>(apu::WAVE_CLOCKS_PER_STEP, (apu::PERIOD_OVERFLOW - wave.frequency) * apu::WAVE_CLOCKS_PER_STEP);
}

uint32_t APU::noisePeriod() const
{
    return static_cast<uint32_t>(apu::NOISE_DIVISORS[reg(io::CHANNEL4_FREQUENCY_REGISTER) & apu::NOISE_DIVISOR_MASK])
        << ((reg(io::CHANNEL4_FREQUENCY_REGISTER) >> apu::NOISE_CLOCK_SHIFT) & apu::NOISE_CLOCK_SHIFT_MASK);
}

void APU::disableIfDacOff(SquareChannel &channel, uint8_t envelopeReg)
{
    if ((envelopeReg & apu::DAC_ENABLE_MASK) == 0) channel.enabled = false;
}

void APU::disableIfDacOff(NoiseChannel &channel, uint8_t envelopeReg)
{
    if ((envelopeReg & apu::DAC_ENABLE_MASK) == 0) channel.enabled = false;
}

void APU::triggerSquare(SquareChannel &channel, bool hasSweep, uint8_t sweepReg)
{
    const uint16_t envelopeRegister = &channel == &square1 ? io::CHANNEL1_ENVELOPE_REGISTER : io::CHANNEL2_ENVELOPE_REGISTER;
    if (channel.length == 0) channel.length = apu::SQUARE_NOISE_MAX_LENGTH;

    channel.enabled = (reg(envelopeRegister) & apu::DAC_ENABLE_MASK) != 0;
    channel.timer = squarePeriod(channel);
    channel.dutyPosition = 0;

    const uint8_t envelope = reg(envelopeRegister);
    channel.initialVolume = envelope >> 4;
    channel.volume = channel.initialVolume;
    channel.envelopeIncrease = checkBit(envelope, apu::ENVELOPE_INCREASE_BIT);
    channel.envelopePeriod = envelope & apu::ENVELOPE_PERIOD_MASK;
    channel.envelopeTimer = channel.envelopePeriod ? channel.envelopePeriod : apu::ZERO_PERIOD_VALUE;

    if (hasSweep)
    {
        const uint8_t shift = sweepReg & apu::SWEEP_SHIFT_MASK;
        const bool negate = checkBit(sweepReg, apu::SWEEP_NEGATE_BIT);

        sweepShadowFrequency = channel.frequency;
        sweepTimer = (sweepReg >> apu::SWEEP_PERIOD_SHIFT) & apu::SWEEP_PERIOD_MASK;

        if (sweepTimer == 0) sweepTimer = apu::ZERO_PERIOD_VALUE;
        sweepEnabled = ((sweepReg & apu::SWEEP_PERIOD_BITS_MASK) != 0) || (shift != 0);
        sweepNegateUsed = false;
        if (shift != 0)
        {
            const uint16_t delta = sweepShadowFrequency >> shift;
            const int calculated = negate
                ? static_cast<int>(sweepShadowFrequency) - delta
                : static_cast<int>(sweepShadowFrequency) + delta;
            if (calculated > apu::MAX_PERIOD) channel.enabled = false;
            if (negate) sweepNegateUsed = true;
        }
    }
}

void APU::triggerWave()
{
    if (wave.length == 0) wave.length = apu::WAVE_MAX_LENGTH;
    wave.enabled = checkBit(reg(io::CHANNEL3_DAC_ENABLE_REGISTER), apu::WAVE_DAC_ENABLE_BIT);
    wave.timer = wavePeriod();
    wave.position = 0;
}

void APU::triggerNoise()
{
    if (noise.length == 0) noise.length = apu::SQUARE_NOISE_MAX_LENGTH;
    noise.enabled = (reg(io::CHANNEL4_ENVELOPE_REGISTER) & apu::DAC_ENABLE_MASK) != 0;
    noise.timer = noisePeriod();
    noise.lfsr = apu::LFSR_RESET_VALUE;

    const uint8_t envelope = reg(io::CHANNEL4_ENVELOPE_REGISTER);
    noise.initialVolume = envelope >> 4;
    noise.volume = noise.initialVolume;
    noise.envelopeIncrease = checkBit(envelope, apu::ENVELOPE_INCREASE_BIT);
    noise.envelopePeriod = envelope & apu::ENVELOPE_PERIOD_MASK;
    noise.envelopeTimer = noise.envelopePeriod ? noise.envelopePeriod : apu::ZERO_PERIOD_VALUE;
}

void APU::clockLength()
{
    auto clockSquare = [](SquareChannel &channel)
    {
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
    auto clockSquare = [](SquareChannel &channel)
    {
        if (channel.envelopePeriod == 0) return;
        if (channel.envelopeTimer > 0) --channel.envelopeTimer;
        if (channel.envelopeTimer == 0)
        {
            channel.envelopeTimer = channel.envelopePeriod;
            if (channel.envelopeIncrease && channel.volume < apu::MAX_VOLUME) ++channel.volume;
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
        if (noise.envelopeIncrease && noise.volume < apu::MAX_VOLUME) ++noise.volume;
        else if (!noise.envelopeIncrease && noise.volume > 0) --noise.volume;
    }
}

void APU::clockSweep()
{
    if (sweepTimer > 0) --sweepTimer;
    if (sweepTimer != 0) return;

    const uint8_t sweep = reg(io::CHANNEL1_SWEEP_REGISTER);
    const bool negate = checkBit(sweep, apu::SWEEP_NEGATE_BIT);
    sweepTimer = (sweep >> apu::SWEEP_PERIOD_SHIFT) & apu::SWEEP_PERIOD_MASK;
    if (sweepTimer == 0) sweepTimer = apu::ZERO_PERIOD_VALUE;
    const uint8_t shift = sweep & apu::SWEEP_SHIFT_MASK;
    if (!sweepEnabled || shift == 0) return;

    const uint16_t delta = sweepShadowFrequency >> shift;
    const int calculated = negate
        ? static_cast<int>(sweepShadowFrequency) - delta
        : static_cast<int>(sweepShadowFrequency) + delta;
    if (negate) sweepNegateUsed = true;
    if (calculated > apu::MAX_PERIOD || calculated < 0)
    {
        square1.enabled = false;
        return;
    }

    // Write the new period back into NR13 and the low bits of NR14
    sweepShadowFrequency = static_cast<uint16_t>(calculated);
    square1.frequency = sweepShadowFrequency;
    setReg(io::CHANNEL1_PERIOD_LOW_REGISTER, static_cast<uint8_t>(calculated));
    setReg(io::CHANNEL1_CONTROL_REGISTER, static_cast<uint8_t>((reg(io::CHANNEL1_CONTROL_REGISTER) & ~apu::PERIOD_HIGH_BITS_MASK) |
                                                         ((calculated >> 8) & apu::PERIOD_HIGH_BITS_MASK)));

    const uint16_t nextDelta = sweepShadowFrequency >> shift;
    const int next = negate
        ? static_cast<int>(sweepShadowFrequency) - nextDelta
        : static_cast<int>(sweepShadowFrequency) + nextDelta;
    if (next > apu::MAX_PERIOD || next < 0) square1.enabled = false;
}

void APU::clockFrameSequencer()
{
    if ((frameSequencerStep & 1) == 0) clockLength();
    if (frameSequencerStep == apu::SWEEP_STEP_A || frameSequencerStep == apu::SWEEP_STEP_B) clockSweep();
    if (frameSequencerStep == apu::ENVELOPE_STEP) clockEnvelopes();
    frameSequencerStep = (frameSequencerStep + 1) & apu::FRAME_SEQUENCER_STEP_MASK;
}

void APU::clockOscillators()
{
    auto clockSquare = [this](SquareChannel &channel)
    {
        if (!channel.enabled) return;
        if (channel.timer > 0) --channel.timer;
        if (channel.timer == 0)
        {
            channel.timer = squarePeriod(channel);
            channel.dutyPosition = (channel.dutyPosition + 1) & apu::DUTY_STEP_MASK;
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
            wave.position = (wave.position + 1) & apu::WAVE_POSITION_MASK;
        }
    }

    if (noise.enabled)
    {
        if (noise.timer > 0) --noise.timer;
        if (noise.timer == 0)
        {
            // Shift the LFSR, feeding XOR of its two lowest bits into
            // the top. Short mode also copies it into bit 6.
            noise.timer = noisePeriod();
            const uint16_t bit = (noise.lfsr ^ (noise.lfsr >> 1)) & 1;
            noise.lfsr = (noise.lfsr >> 1) | (bit << apu::LFSR_HIGH_BIT);
            if (checkBit(reg(io::CHANNEL4_FREQUENCY_REGISTER), apu::NOISE_SHORT_MODE_BIT))
                noise.lfsr = (noise.lfsr & ~(1u << apu::LFSR_SHORT_MODE_BIT)) | (bit << apu::LFSR_SHORT_MODE_BIT);
        }
    }
}

int APU::squareOutput(const SquareChannel &channel, uint8_t dutyReg) const
{
    if (!channel.enabled) return 0;
    const uint8_t duty = (dutyReg >> apu::DUTY_SHIFT) & apu::DUTY_MASK;
    const int bit = (apu::DUTY_PATTERNS[duty] >> channel.dutyPosition) & 1;
    return bit ? channel.volume : 0;
}

int APU::waveOutput() const
{
    if (!wave.enabled) return 0;
    const uint8_t level = (reg(io::CHANNEL3_OUTPUT_LEVEL_REGISTER) >> apu::WAVE_LEVEL_SHIFT) & apu::WAVE_LEVEL_MASK;
    if (level == 0) return 0;

    // Each byte holds two samples, high nibble first
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
    const int outputs[apu::CHANNEL_COUNT] =
    {
        squareOutput(square1, reg(io::CHANNEL1_LENGTH_DUTY_REGISTER)),
        squareOutput(square2, reg(io::CHANNEL2_LENGTH_DUTY_REGISTER)),
        waveOutput(),
        noiseOutput()
    };

    const bool active[apu::CHANNEL_COUNT] = {square1.enabled, square2.enabled, wave.enabled, noise.enabled};
    const uint8_t routing = reg(io::SOUND_PANNING_REGISTER);

    int left = 0;
    int right = 0;
    int leftChannels = 0;
    int rightChannels = 0;
    for (int channel = 0; channel < apu::CHANNEL_COUNT; ++channel)
    {
        const int output = active[channel] ? outputs[channel] * 2 - apu::CHANNEL_OUTPUT_CENTER : 0;
        if ((routing & (1 << channel)) && active[channel])
        {
            right += output;
            ++rightChannels;
        }
        if ((routing & (1 << (channel + apu::LEFT_PANNING_SHIFT))) && active[channel])
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
    const uint8_t volume = reg(io::MASTER_VOLUME_REGISTER);
    left *= (((volume >> apu::LEFT_VOLUME_SHIFT) & apu::MASTER_VOLUME_MASK) + 1) * apu::OUTPUT_SCALE;
    right *= ((volume & apu::MASTER_VOLUME_MASK) + 1) * apu::OUTPUT_SCALE;

    // Approximate the DMG output coupling capacitors, which remove DC bias
    // and soften the large offsets produced by pulse-wave duty cycles.
    constexpr int32_t HIGH_PASS_COEFFICIENT = 65274; // 0.996 in Q16
    const int32_t filteredLeft = left - previousInputLeft
        + static_cast<int32_t>((static_cast<int64_t>(previousOutputLeft) * HIGH_PASS_COEFFICIENT) >> 16);
    const int32_t filteredRight = right - previousInputRight
        + static_cast<int32_t>((static_cast<int64_t>(previousOutputRight) * HIGH_PASS_COEFFICIENT) >> 16);

    previousInputLeft = left;
    previousInputRight = right;
    previousOutputLeft = filteredLeft;
    previousOutputRight = filteredRight;
    left = filteredLeft;
    right = filteredRight;

    constexpr int SAMPLE_MIN = std::numeric_limits<int16_t>::min();
    constexpr int SAMPLE_MAX = std::numeric_limits<int16_t>::max();
    left = std::max(SAMPLE_MIN, std::min(SAMPLE_MAX, left));
    right = std::max(SAMPLE_MIN, std::min(SAMPLE_MAX, right));
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
            if (++frameSequencerCycles >= apu::FRAME_SEQUENCER_PERIOD)
            {
                frameSequencerCycles = 0;
                clockFrameSequencer();
            }
        }

        // Emit an output sample every (clock rate / sample rate)
        // clocks, averaging everything accumulated since the last one
        accumulateSample();
        samplePhase += sampleRate;
        if (samplePhase >= static_cast<uint32_t>(common::GAMEBOY_CLOCK_RATE))
        {
            samplePhase -= common::GAMEBOY_CLOCK_RATE;
            emitSample();
        }
    }
}

uint8_t APU::read(uint16_t address) const
{
    // While the wave channel plays, wave RAM reads
    // return the byte it is currently playing
    if (address >= io::WAVE_RAM_START)
    {
        const uint8_t index = wave.enabled ? (wave.position >> 1) : (address - io::WAVE_RAM_START);
        return waveRAM[index];
    }
    if (address == io::SOUND_CONTROL_REGISTER)
    {
        return static_cast<uint8_t>(apu::SOUND_CONTROL_UNUSED_BITS | (powered ? 1 << apu::POWER_BIT : 0)
            | (square1.enabled ? 1 : 0) | (square2.enabled ? 2 : 0)
            | (wave.enabled ? 4 : 0) | (noise.enabled ? 8 : 0));
    }

    // 0xFF27-0xFF2F are unused
    if (address > io::SOUND_CONTROL_REGISTER) return io::UNMAPPED_READ_VALUE;

    // Bits that always read as 1 in NR10-NR51: write-only
    // bits (lengths, periods, triggers) and unused bits.
    // 0xFF15 and 0xFF1F are unused registers.
    static constexpr uint8_t READ_MASKS[io::SOUND_CONTROL_REGISTER - io::AUDIO_ADDRESS_START] =
    {
        0x80, 0x3F, 0x00, 0xFF, 0xBF, 0xFF, 0x3F, 0x00,
        0xFF, 0xBF, 0x7F, 0xFF, 0x9F, 0xFF, 0xBF, 0xFF,
        0x3F, 0x00, 0x00, 0xBF, 0x00, 0x00
    };
    return reg(address) | READ_MASKS[address - io::AUDIO_ADDRESS_START];
}

void APU::write(uint16_t address, uint8_t value)
{
    // While the wave channel plays, wave RAM writes
    // go to the byte it is currently playing
    if (address >= io::WAVE_RAM_START)
    {
        const uint8_t index = wave.enabled ? (wave.position >> 1) : (address - io::WAVE_RAM_START);
        waveRAM[index] = value;
        return;
    }
    if (address == io::SOUND_CONTROL_REGISTER)
    {
        const bool newPower = checkBit(value, apu::POWER_BIT);
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

    if (address < io::CHANNEL1_SWEEP_REGISTER || address > io::SOUND_PANNING_REGISTER) return;

    // While powered off, only the length counters can be written
    if (!powered)
    {
        if (address == io::CHANNEL1_LENGTH_DUTY_REGISTER) square1.length = apu::SQUARE_NOISE_MAX_LENGTH - (value & apu::LENGTH_MASK);
        else if (address == io::CHANNEL2_LENGTH_DUTY_REGISTER) square2.length = apu::SQUARE_NOISE_MAX_LENGTH - (value & apu::LENGTH_MASK);
        else if (address == io::CHANNEL3_LENGTH_REGISTER) wave.length = apu::WAVE_MAX_LENGTH - value;
        else if (address == io::CHANNEL4_LENGTH_REGISTER) noise.length = apu::SQUARE_NOISE_MAX_LENGTH - (value & apu::LENGTH_MASK);
        return;
    }

    const uint8_t oldValue = reg(address);
    setReg(address, value);
    switch (address)
    {
        // Switching the sweep from subtract to add after
        // it has subtracted disables channel 1
        case io::CHANNEL1_SWEEP_REGISTER:
            if (sweepNegateUsed && checkBit(oldValue, apu::SWEEP_NEGATE_BIT) && !checkBit(value, apu::SWEEP_NEGATE_BIT))
                square1.enabled = false;
            break;
        case io::CHANNEL1_LENGTH_DUTY_REGISTER: square1.length = apu::SQUARE_NOISE_MAX_LENGTH - (value & apu::LENGTH_MASK); break;
        case io::CHANNEL1_ENVELOPE_REGISTER: disableIfDacOff(square1, value); break;
        case io::CHANNEL1_PERIOD_LOW_REGISTER: square1.frequency = (square1.frequency & apu::PERIOD_HIGH_MASK) | value; break;
        case io::CHANNEL1_CONTROL_REGISTER:
            square1.frequency = (square1.frequency & 0xFF) | ((value & apu::PERIOD_HIGH_BITS_MASK) << 8);
            square1.lengthEnabled = checkBit(value, apu::LENGTH_ENABLE_BIT);
            if (checkBit(value, apu::TRIGGER_BIT)) triggerSquare(square1, true, reg(io::CHANNEL1_SWEEP_REGISTER));
            break;
        case io::CHANNEL2_LENGTH_DUTY_REGISTER: square2.length = apu::SQUARE_NOISE_MAX_LENGTH - (value & apu::LENGTH_MASK); break;
        case io::CHANNEL2_ENVELOPE_REGISTER: disableIfDacOff(square2, value); break;
        case io::CHANNEL2_PERIOD_LOW_REGISTER: square2.frequency = (square2.frequency & apu::PERIOD_HIGH_MASK) | value; break;
        case io::CHANNEL2_CONTROL_REGISTER:
            square2.frequency = (square2.frequency & 0xFF) | ((value & apu::PERIOD_HIGH_BITS_MASK) << 8);
            square2.lengthEnabled = checkBit(value, apu::LENGTH_ENABLE_BIT);
            if (checkBit(value, apu::TRIGGER_BIT)) triggerSquare(square2, false, 0);
            break;
        case io::CHANNEL3_DAC_ENABLE_REGISTER: if (!checkBit(value, apu::WAVE_DAC_ENABLE_BIT)) wave.enabled = false; break;
        case io::CHANNEL3_LENGTH_REGISTER: wave.length = apu::WAVE_MAX_LENGTH - value; break;
        case io::CHANNEL3_OUTPUT_LEVEL_REGISTER: break;
        case io::CHANNEL3_PERIOD_LOW_REGISTER: wave.frequency = (wave.frequency & apu::PERIOD_HIGH_MASK) | value; break;
        case io::CHANNEL3_CONTROL_REGISTER:
            wave.frequency = (wave.frequency & 0xFF) | ((value & apu::PERIOD_HIGH_BITS_MASK) << 8);
            wave.lengthEnabled = checkBit(value, apu::LENGTH_ENABLE_BIT);
            if (checkBit(value, apu::TRIGGER_BIT)) triggerWave();
            break;
        case io::CHANNEL4_LENGTH_REGISTER: noise.length = apu::SQUARE_NOISE_MAX_LENGTH - (value & apu::LENGTH_MASK); break;
        case io::CHANNEL4_ENVELOPE_REGISTER: disableIfDacOff(noise, value); break;
        case io::CHANNEL4_FREQUENCY_REGISTER: break;
        case io::CHANNEL4_CONTROL_REGISTER:
            noise.lengthEnabled = checkBit(value, apu::LENGTH_ENABLE_BIT);
            if (checkBit(value, apu::TRIGGER_BIT)) triggerNoise();
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
