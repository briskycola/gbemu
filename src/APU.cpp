#include "APU.hpp"
#include "Common.hpp"
#include <algorithm>
#include <limits>

// https://gbdev.io/pandocs/Audio_Registers.html

namespace
{
// The frame sequencer runs at 512 Hz and clocks the length
// counters, volume envelopes, and frequency sweep. It has
// 8 steps: lengths on even steps, sweep on steps 2 and 6,
// and envelopes on step 7.
constexpr uint32_t FRAME_SEQUENCER_PERIOD = 8192;
constexpr uint8_t FRAME_SEQUENCER_STEP_MASK = 0x07;
constexpr uint8_t SWEEP_STEP_A = 2;
constexpr uint8_t SWEEP_STEP_B = 6;
constexpr uint8_t ENVELOPE_STEP = 7;

// Square wave duty patterns, one bit per step:
// 12.5%, 25%, 50%, and 75%
constexpr uint8_t DUTY_PATTERNS[4] = {0x01, 0x81, 0x87, 0x7E};
constexpr uint8_t DUTY_STEP_MASK = 0x07;

// Noise channel clock divisors, indexed by NR43 bits 0-2
constexpr uint8_t NOISE_DIVISORS[8] = {8, 16, 32, 48, 64, 80, 96, 112};

// Channel periods are 11-bit values. The timer counts up
// from the period to 2048; square channels step every 4
// clocks and the wave channel every 2.
constexpr uint32_t PERIOD_OVERFLOW = 2048;
constexpr int MAX_PERIOD = 2047;
constexpr uint32_t SQUARE_CLOCKS_PER_STEP = 4;
constexpr uint32_t WAVE_CLOCKS_PER_STEP = 2;
constexpr uint16_t PERIOD_HIGH_MASK = 0x700; // Bits 8-10, written through NRx4
constexpr uint8_t PERIOD_HIGH_BITS_MASK = 0x07; // Bits 0-2 of NRx4

// NRx4 bits shared by every channel
constexpr uint8_t LENGTH_ENABLE_BIT = 6;
constexpr uint8_t TRIGGER_BIT = 7;

// Length counters count down from these values. Writes
// to NRx1 set the length to (max - value).
constexpr uint16_t SQUARE_NOISE_MAX_LENGTH = 64;
constexpr uint16_t WAVE_MAX_LENGTH = 256;
constexpr uint8_t LENGTH_MASK = 0x3F; // Bits 0-5 of NR11/NR21/NR41

// NRx2 volume envelope: bits 4-7 are the starting volume,
// bit 3 the direction, and bits 0-2 the period. The DAC is
// on while any of bits 3-7 is set.
constexpr uint8_t DAC_ENABLE_MASK = 0xF8;
constexpr uint8_t ENVELOPE_INCREASE_BIT = 3;
constexpr uint8_t ENVELOPE_PERIOD_MASK = 0x07;
constexpr uint8_t MAX_VOLUME = 15;

// Envelope and sweep periods of 0 are treated as 8
constexpr uint8_t ZERO_PERIOD_VALUE = 8;

// NR10 sweep: bits 4-6 are the period, bit 3 the
// direction (1 = subtract), and bits 0-2 the shift
constexpr uint8_t SWEEP_PERIOD_SHIFT = 4;
constexpr uint8_t SWEEP_PERIOD_MASK = 0x07;
constexpr uint8_t SWEEP_PERIOD_BITS_MASK = 0x70;
constexpr uint8_t SWEEP_NEGATE_BIT = 3;
constexpr uint8_t SWEEP_SHIFT_MASK = 0x07;

// NR30 bit 7 turns the wave channel's DAC on
constexpr uint8_t WAVE_DAC_ENABLE_BIT = 7;

// Wave RAM holds 32 4-bit samples
constexpr uint8_t WAVE_POSITION_MASK = 0x1F;

// NR32 bits 5-6 set the wave volume: 0 = mute,
// 1 = 100%, 2 = 50%, and 3 = 25%
constexpr uint8_t WAVE_LEVEL_SHIFT = 5;
constexpr uint8_t WAVE_LEVEL_MASK = 0x03;

// NR43: bits 0-2 pick the divisor, bit 3 selects the 7-bit
// LFSR, and bits 4-7 are the clock shift. The LFSR is 15 bits
// wide and resets to all ones.
constexpr uint8_t NOISE_DIVISOR_MASK = 0x07;
constexpr uint8_t NOISE_SHORT_MODE_BIT = 3;
constexpr uint8_t NOISE_CLOCK_SHIFT = 4;
constexpr uint8_t NOISE_CLOCK_SHIFT_MASK = 0x0F;
constexpr uint16_t LFSR_RESET_VALUE = 0x7FFF;
constexpr uint8_t LFSR_HIGH_BIT = 14;
constexpr uint8_t LFSR_SHORT_MODE_BIT = 6;

// NRx1 bits 6-7 pick the square duty pattern
constexpr uint8_t DUTY_SHIFT = 6;
constexpr uint8_t DUTY_MASK = 0x03;

// NR50 master volume: bits 4-6 are the left volume
// and bits 0-2 the right, each 0-7 meaning 1/8 to 8/8
constexpr uint8_t LEFT_VOLUME_SHIFT = 4;
constexpr uint8_t MASTER_VOLUME_MASK = 0x07;

// NR51 panning: bits 0-3 send channels 1-4 to the
// right speaker, and bits 4-7 to the left
constexpr uint8_t CHANNEL_COUNT = 4;
constexpr uint8_t LEFT_PANNING_SHIFT = 4;

// Each channel outputs 0-15. Doubling and subtracting 15 centers it
// around 0, and the summed result is scaled up to the 16-bit range.
constexpr int CHANNEL_OUTPUT_CENTER = 15;
constexpr int OUTPUT_SCALE = 64;

// NR52: bit 7 is the power switch and bits 0-3 report which
// channels are playing. Bits 4-6 are unused and read as 1.
constexpr uint8_t POWER_BIT = 7;
constexpr uint8_t SOUND_CONTROL_UNUSED_BITS = 0x70;
}

uint8_t APU::reg(uint16_t address) const
{
    return registers[address - AUDIO_ADDRESS_START];
}

void APU::setReg(uint16_t address, uint8_t value)
{
    registers[address - AUDIO_ADDRESS_START] = value;
}

uint32_t APU::squarePeriod(const SquareChannel &channel) const
{
    return std::max<uint32_t>(SQUARE_CLOCKS_PER_STEP, (PERIOD_OVERFLOW - channel.frequency) * SQUARE_CLOCKS_PER_STEP);
}

uint32_t APU::wavePeriod() const
{
    return std::max<uint32_t>(WAVE_CLOCKS_PER_STEP, (PERIOD_OVERFLOW - wave.frequency) * WAVE_CLOCKS_PER_STEP);
}

uint32_t APU::noisePeriod() const
{
    return static_cast<uint32_t>(NOISE_DIVISORS[reg(CHANNEL4_FREQUENCY_REGISTER) & NOISE_DIVISOR_MASK])
        << ((reg(CHANNEL4_FREQUENCY_REGISTER) >> NOISE_CLOCK_SHIFT) & NOISE_CLOCK_SHIFT_MASK);
}

void APU::disableIfDacOff(SquareChannel &channel, uint8_t envelopeReg)
{
    if ((envelopeReg & DAC_ENABLE_MASK) == 0) channel.enabled = false;
}

void APU::disableIfDacOff(NoiseChannel &channel, uint8_t envelopeReg)
{
    if ((envelopeReg & DAC_ENABLE_MASK) == 0) channel.enabled = false;
}

void APU::triggerSquare(SquareChannel &channel, bool hasSweep, uint8_t sweepReg)
{
    const uint16_t envelopeRegister = &channel == &square1 ? CHANNEL1_ENVELOPE_REGISTER : CHANNEL2_ENVELOPE_REGISTER;
    if (channel.length == 0) channel.length = SQUARE_NOISE_MAX_LENGTH;
    channel.enabled = (reg(envelopeRegister) & DAC_ENABLE_MASK) != 0;
    channel.timer = squarePeriod(channel);
    channel.dutyPosition = 0;
    const uint8_t envelope = reg(envelopeRegister);
    channel.initialVolume = envelope >> 4;
    channel.volume = channel.initialVolume;
    channel.envelopeIncrease = checkBit(envelope, ENVELOPE_INCREASE_BIT);
    channel.envelopePeriod = envelope & ENVELOPE_PERIOD_MASK;
    channel.envelopeTimer = channel.envelopePeriod ? channel.envelopePeriod : ZERO_PERIOD_VALUE;

    if (hasSweep)
    {
        const uint8_t shift = sweepReg & SWEEP_SHIFT_MASK;
        const bool negate = checkBit(sweepReg, SWEEP_NEGATE_BIT);
        sweepShadowFrequency = channel.frequency;
        sweepTimer = (sweepReg >> SWEEP_PERIOD_SHIFT) & SWEEP_PERIOD_MASK;
        if (sweepTimer == 0) sweepTimer = ZERO_PERIOD_VALUE;
        sweepEnabled = ((sweepReg & SWEEP_PERIOD_BITS_MASK) != 0) || (shift != 0);
        sweepNegateUsed = false;
        if (shift != 0)
        {
            const uint16_t delta = sweepShadowFrequency >> shift;
            const int calculated = negate
                ? static_cast<int>(sweepShadowFrequency) - delta
                : static_cast<int>(sweepShadowFrequency) + delta;
            if (calculated > MAX_PERIOD) channel.enabled = false;
            if (negate) sweepNegateUsed = true;
        }
    }
}

void APU::triggerWave()
{
    if (wave.length == 0) wave.length = WAVE_MAX_LENGTH;
    wave.enabled = checkBit(reg(CHANNEL3_DAC_ENABLE_REGISTER), WAVE_DAC_ENABLE_BIT);
    wave.timer = wavePeriod();
    wave.position = 0;
}

void APU::triggerNoise()
{
    if (noise.length == 0) noise.length = SQUARE_NOISE_MAX_LENGTH;
    noise.enabled = (reg(CHANNEL4_ENVELOPE_REGISTER) & DAC_ENABLE_MASK) != 0;
    noise.timer = noisePeriod();
    noise.lfsr = LFSR_RESET_VALUE;
    const uint8_t envelope = reg(CHANNEL4_ENVELOPE_REGISTER);
    noise.initialVolume = envelope >> 4;
    noise.volume = noise.initialVolume;
    noise.envelopeIncrease = checkBit(envelope, ENVELOPE_INCREASE_BIT);
    noise.envelopePeriod = envelope & ENVELOPE_PERIOD_MASK;
    noise.envelopeTimer = noise.envelopePeriod ? noise.envelopePeriod : ZERO_PERIOD_VALUE;
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
            if (channel.envelopeIncrease && channel.volume < MAX_VOLUME) ++channel.volume;
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
        if (noise.envelopeIncrease && noise.volume < MAX_VOLUME) ++noise.volume;
        else if (!noise.envelopeIncrease && noise.volume > 0) --noise.volume;
    }
}

void APU::clockSweep()
{
    if (sweepTimer > 0) --sweepTimer;
    if (sweepTimer != 0) return;

    const uint8_t sweep = reg(CHANNEL1_SWEEP_REGISTER);
    const bool negate = checkBit(sweep, SWEEP_NEGATE_BIT);
    sweepTimer = (sweep >> SWEEP_PERIOD_SHIFT) & SWEEP_PERIOD_MASK;
    if (sweepTimer == 0) sweepTimer = ZERO_PERIOD_VALUE;
    const uint8_t shift = sweep & SWEEP_SHIFT_MASK;
    if (!sweepEnabled || shift == 0) return;

    const uint16_t delta = sweepShadowFrequency >> shift;
    const int calculated = negate
        ? static_cast<int>(sweepShadowFrequency) - delta
        : static_cast<int>(sweepShadowFrequency) + delta;
    if (negate) sweepNegateUsed = true;
    if (calculated > MAX_PERIOD || calculated < 0)
    {
        square1.enabled = false;
        return;
    }

    // Write the new period back into NR13 and the low bits of NR14
    sweepShadowFrequency = static_cast<uint16_t>(calculated);
    square1.frequency = sweepShadowFrequency;
    setReg(CHANNEL1_PERIOD_LOW_REGISTER, static_cast<uint8_t>(calculated));
    setReg(CHANNEL1_CONTROL_REGISTER, static_cast<uint8_t>((reg(CHANNEL1_CONTROL_REGISTER) & ~PERIOD_HIGH_BITS_MASK) |
                                                         ((calculated >> 8) & PERIOD_HIGH_BITS_MASK)));

    const uint16_t nextDelta = sweepShadowFrequency >> shift;
    const int next = negate
        ? static_cast<int>(sweepShadowFrequency) - nextDelta
        : static_cast<int>(sweepShadowFrequency) + nextDelta;
    if (next > MAX_PERIOD || next < 0) square1.enabled = false;
}

void APU::clockFrameSequencer()
{
    if ((frameSequencerStep & 1) == 0) clockLength();
    if (frameSequencerStep == SWEEP_STEP_A || frameSequencerStep == SWEEP_STEP_B) clockSweep();
    if (frameSequencerStep == ENVELOPE_STEP) clockEnvelopes();
    frameSequencerStep = (frameSequencerStep + 1) & FRAME_SEQUENCER_STEP_MASK;
}

void APU::clockOscillators()
{
    auto clockSquare = [this](SquareChannel &channel) {
        if (!channel.enabled) return;
        if (channel.timer > 0) --channel.timer;
        if (channel.timer == 0)
        {
            channel.timer = squarePeriod(channel);
            channel.dutyPosition = (channel.dutyPosition + 1) & DUTY_STEP_MASK;
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
            wave.position = (wave.position + 1) & WAVE_POSITION_MASK;
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
            noise.lfsr = (noise.lfsr >> 1) | (bit << LFSR_HIGH_BIT);
            if (checkBit(reg(CHANNEL4_FREQUENCY_REGISTER), NOISE_SHORT_MODE_BIT))
                noise.lfsr = (noise.lfsr & ~(1u << LFSR_SHORT_MODE_BIT)) | (bit << LFSR_SHORT_MODE_BIT);
        }
    }
}

int APU::squareOutput(const SquareChannel &channel, uint8_t dutyReg) const
{
    if (!channel.enabled) return 0;
    const uint8_t duty = (dutyReg >> DUTY_SHIFT) & DUTY_MASK;
    const int bit = (DUTY_PATTERNS[duty] >> channel.dutyPosition) & 1;
    return bit ? channel.volume : 0;
}

int APU::waveOutput() const
{
    if (!wave.enabled) return 0;
    const uint8_t level = (reg(CHANNEL3_OUTPUT_LEVEL_REGISTER) >> WAVE_LEVEL_SHIFT) & WAVE_LEVEL_MASK;
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
    const int outputs[CHANNEL_COUNT] = {
        squareOutput(square1, reg(CHANNEL1_LENGTH_DUTY_REGISTER)),
        squareOutput(square2, reg(CHANNEL2_LENGTH_DUTY_REGISTER)),
        waveOutput(),
        noiseOutput()
    };
    const bool active[CHANNEL_COUNT] = {square1.enabled, square2.enabled, wave.enabled, noise.enabled};
    const uint8_t routing = reg(SOUND_PANNING_REGISTER);
    int left = 0;
    int right = 0;
    int leftChannels = 0;
    int rightChannels = 0;
    for (int channel = 0; channel < CHANNEL_COUNT; ++channel)
    {
        const int output = active[channel] ? outputs[channel] * 2 - CHANNEL_OUTPUT_CENTER : 0;
        if ((routing & (1 << channel)) && active[channel])
        {
            right += output;
            ++rightChannels;
        }
        if ((routing & (1 << (channel + LEFT_PANNING_SHIFT))) && active[channel])
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
    const uint8_t volume = reg(MASTER_VOLUME_REGISTER);
    left *= (((volume >> LEFT_VOLUME_SHIFT) & MASTER_VOLUME_MASK) + 1) * OUTPUT_SCALE;
    right *= ((volume & MASTER_VOLUME_MASK) + 1) * OUTPUT_SCALE;

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
            if (++frameSequencerCycles >= FRAME_SEQUENCER_PERIOD)
            {
                frameSequencerCycles = 0;
                clockFrameSequencer();
            }
        }

        // Emit an output sample every (clock rate / sample rate)
        // clocks, averaging everything accumulated since the last one
        accumulateSample();
        samplePhase += sampleRate;
        if (samplePhase >= static_cast<uint32_t>(GAMEBOY_CLOCK_RATE))
        {
            samplePhase -= GAMEBOY_CLOCK_RATE;
            emitSample();
        }
    }
}

uint8_t APU::read(uint16_t address) const
{
    // While the wave channel plays, wave RAM reads
    // return the byte it is currently playing
    if (address >= WAVE_RAM_START)
    {
        const uint8_t index = wave.enabled ? (wave.position >> 1) : (address - WAVE_RAM_START);
        return waveRAM[index];
    }
    if (address == SOUND_CONTROL_REGISTER)
    {
        return static_cast<uint8_t>(SOUND_CONTROL_UNUSED_BITS | (powered ? 1 << POWER_BIT : 0)
            | (square1.enabled ? 1 : 0) | (square2.enabled ? 2 : 0)
            | (wave.enabled ? 4 : 0) | (noise.enabled ? 8 : 0));
    }

    // 0xFF27-0xFF2F are unused
    if (address > SOUND_CONTROL_REGISTER) return UNMAPPED_READ_VALUE;

    // Bits that always read as 1 in NR10-NR51: write-only
    // bits (lengths, periods, triggers) and unused bits.
    // 0xFF15 and 0xFF1F are unused registers.
    static constexpr uint8_t READ_MASKS[SOUND_CONTROL_REGISTER - AUDIO_ADDRESS_START] = {
        0x80, 0x3F, 0x00, 0xFF, 0xBF, 0xFF, 0x3F, 0x00,
        0xFF, 0xBF, 0x7F, 0xFF, 0x9F, 0xFF, 0xBF, 0xFF,
        0x3F, 0x00, 0x00, 0xBF, 0x00, 0x00
    };
    return reg(address) | READ_MASKS[address - AUDIO_ADDRESS_START];
}

void APU::write(uint16_t address, uint8_t value)
{
    // While the wave channel plays, wave RAM writes
    // go to the byte it is currently playing
    if (address >= WAVE_RAM_START)
    {
        const uint8_t index = wave.enabled ? (wave.position >> 1) : (address - WAVE_RAM_START);
        waveRAM[index] = value;
        return;
    }
    if (address == SOUND_CONTROL_REGISTER)
    {
        const bool newPower = checkBit(value, POWER_BIT);
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

    if (address < CHANNEL1_SWEEP_REGISTER || address > SOUND_PANNING_REGISTER) return;

    // While powered off, only the length counters can be written
    if (!powered)
    {
        if (address == CHANNEL1_LENGTH_DUTY_REGISTER) square1.length = SQUARE_NOISE_MAX_LENGTH - (value & LENGTH_MASK);
        else if (address == CHANNEL2_LENGTH_DUTY_REGISTER) square2.length = SQUARE_NOISE_MAX_LENGTH - (value & LENGTH_MASK);
        else if (address == CHANNEL3_LENGTH_REGISTER) wave.length = WAVE_MAX_LENGTH - value;
        else if (address == CHANNEL4_LENGTH_REGISTER) noise.length = SQUARE_NOISE_MAX_LENGTH - (value & LENGTH_MASK);
        return;
    }

    const uint8_t oldValue = reg(address);
    setReg(address, value);
    switch (address)
    {
        // Switching the sweep from subtract to add after
        // it has subtracted disables channel 1
        case CHANNEL1_SWEEP_REGISTER:
            if (sweepNegateUsed && checkBit(oldValue, SWEEP_NEGATE_BIT) && !checkBit(value, SWEEP_NEGATE_BIT))
                square1.enabled = false;
            break;
        case CHANNEL1_LENGTH_DUTY_REGISTER: square1.length = SQUARE_NOISE_MAX_LENGTH - (value & LENGTH_MASK); break;
        case CHANNEL1_ENVELOPE_REGISTER: disableIfDacOff(square1, value); break;
        case CHANNEL1_PERIOD_LOW_REGISTER: square1.frequency = (square1.frequency & PERIOD_HIGH_MASK) | value; break;
        case CHANNEL1_CONTROL_REGISTER:
            square1.frequency = (square1.frequency & 0xFF) | ((value & PERIOD_HIGH_BITS_MASK) << 8);
            square1.lengthEnabled = checkBit(value, LENGTH_ENABLE_BIT);
            if (checkBit(value, TRIGGER_BIT)) triggerSquare(square1, true, reg(CHANNEL1_SWEEP_REGISTER));
            break;
        case CHANNEL2_LENGTH_DUTY_REGISTER: square2.length = SQUARE_NOISE_MAX_LENGTH - (value & LENGTH_MASK); break;
        case CHANNEL2_ENVELOPE_REGISTER: disableIfDacOff(square2, value); break;
        case CHANNEL2_PERIOD_LOW_REGISTER: square2.frequency = (square2.frequency & PERIOD_HIGH_MASK) | value; break;
        case CHANNEL2_CONTROL_REGISTER:
            square2.frequency = (square2.frequency & 0xFF) | ((value & PERIOD_HIGH_BITS_MASK) << 8);
            square2.lengthEnabled = checkBit(value, LENGTH_ENABLE_BIT);
            if (checkBit(value, TRIGGER_BIT)) triggerSquare(square2, false, 0);
            break;
        case CHANNEL3_DAC_ENABLE_REGISTER: if (!checkBit(value, WAVE_DAC_ENABLE_BIT)) wave.enabled = false; break;
        case CHANNEL3_LENGTH_REGISTER: wave.length = WAVE_MAX_LENGTH - value; break;
        case CHANNEL3_OUTPUT_LEVEL_REGISTER: break;
        case CHANNEL3_PERIOD_LOW_REGISTER: wave.frequency = (wave.frequency & PERIOD_HIGH_MASK) | value; break;
        case CHANNEL3_CONTROL_REGISTER:
            wave.frequency = (wave.frequency & 0xFF) | ((value & PERIOD_HIGH_BITS_MASK) << 8);
            wave.lengthEnabled = checkBit(value, LENGTH_ENABLE_BIT);
            if (checkBit(value, TRIGGER_BIT)) triggerWave();
            break;
        case CHANNEL4_LENGTH_REGISTER: noise.length = SQUARE_NOISE_MAX_LENGTH - (value & LENGTH_MASK); break;
        case CHANNEL4_ENVELOPE_REGISTER: disableIfDacOff(noise, value); break;
        case CHANNEL4_FREQUENCY_REGISTER: break;
        case CHANNEL4_CONTROL_REGISTER:
            noise.lengthEnabled = checkBit(value, LENGTH_ENABLE_BIT);
            if (checkBit(value, TRIGGER_BIT)) triggerNoise();
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
