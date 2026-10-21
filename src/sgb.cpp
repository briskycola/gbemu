#include "sgb.hpp"
#include <algorithm>

SGB::SGB()
{
    const uint16_t initial[4] = {0x7FFF, 0x56B5, 0x294A, 0x0000};
    for (uint8_t p = 0; p < 4; ++p)
        for (uint8_t c = 0; c < 4; ++c) palettes[p * 4 + c] = rgb15(initial[c]);
    attributes.fill(0);
    previousFrame.fill(0xFFFFFF);
}

uint32_t SGB::rgb15(uint16_t value) const
{
    const uint8_t r5 = value & 0x1F;
    const uint8_t g5 = (value >> 5) & 0x1F;
    const uint8_t b5 = (value >> 10) & 0x1F;
    return ((r5 << 3 | r5 >> 2) << 16) | ((g5 << 3 | g5 >> 2) << 8) | (b5 << 3 | b5 >> 2);
}

void SGB::writeJOYP(uint8_t value)
{
    const uint8_t select = (value >> 4) & 3;
    if ((value & 0x20) && !(previousJoyp & 0x20) && playerCount > 1)
        currentPlayer = (currentPlayer + 1) & (playerCount - 1);
    previousJoyp = value;
    if (select == 3) { readyForPulse = true; return; }

    if (select == 0)
    {
        if (readyForPulse)
        {
            readyForWrite = true;
            readyForPulse = false;
            if ((bitCount & 127) != 0 || bitCount == 0)
            {
                bitCount = 0;
                command.fill(0);
                readyForStop = false;
            }
        }
        return;
    }

    if (!readyForPulse || !readyForWrite) return;
    readyForPulse = false;
    if (readyForStop)
    {
        if (select != 2)
        {
            bitCount = 0;
            command.fill(0);
            readyForStop = false;
            readyForWrite = false;
            return;
        }
        const uint16_t packets = (command[0] & 7) ? (command[0] & 7) : 1;
        if (bitCount >= packets * 128)
        {
            commandReady();
            bitCount = 0;
            command.fill(0);
        }
        readyForStop = false;
        readyForWrite = false;
        return;
    }

    if (bitCount < command.size() * 8 && select == 1)
        command[bitCount >> 3] |= static_cast<uint8_t>(1u << (bitCount & 7));
    if (bitCount < command.size() * 8) ++bitCount;
    if ((bitCount & 127) == 0) readyForStop = true;
}

void SGB::commandReady()
{
    const uint8_t op = command[0] >> 3;
    if (op <= 3)
    {
        const uint8_t first = op == 0 ? 0 : (op == 1 ? 2 : (op == 2 ? 0 : 1));
        const uint8_t second = op == 0 ? 1 : (op == 1 ? 3 : (op == 2 ? 3 : 2));
        const uint32_t color0 = rgb15(command[1] | (command[2] << 8));
        for (uint8_t p = 0; p < 4; ++p) palettes[p * 4] = color0;
        for (uint8_t i = 0; i < 3; ++i)
        {
            palettes[first * 4 + i + 1] = rgb15(command[3 + i * 2] | (command[4 + i * 2] << 8));
            palettes[second * 4 + i + 1] = rgb15(command[9 + i * 2] | (command[10 + i * 2] << 8));
        }
        return;
    }

    if (op == 5)
    {
        const uint8_t count = std::min<uint8_t>(command[1], 110);
        for (uint8_t i = 0; i < count; ++i)
        {
            const uint8_t data = command[2 + i];
            const uint8_t palette = (data >> 5) & 3;
            const uint8_t line = data & 0x1F;
            if (data & 0x80)
            {
                if (line < 18) for (uint8_t x = 0; x < 20; ++x) attributes[line * 20 + x] = palette;
            }
            else if (line < 20) for (uint8_t y = 0; y < 18; ++y) attributes[y * 20 + line] = palette;
        }
        return;
    }

    if (op == 6)
    {
        const uint8_t mode = command[1];
        const uint8_t low = (mode >> 2) & 3;
        const uint8_t middle = (mode >> 4) & 3;
        const uint8_t high = mode & 3;
        const uint8_t line = command[2] & 0x1F;
        for (uint8_t y = 0; y < 18; ++y) for (uint8_t x = 0; x < 20; ++x)
        {
            const uint8_t coord = (mode & 0x40) ? y : x;
            attributes[y * 20 + x] = coord < line ? low : (coord == line ? middle : high);
        }
        return;
    }

    if (op == 7)
    {
        uint8_t x = command[1], y = command[2];
        const uint16_t count = command[3] | (command[4] << 8);
        const bool vertical = (command[5] & 1) != 0;
        for (uint16_t i = 0; i < count && i < 360; ++i)
        {
            const uint8_t palette = (command[6 + i / 4] >> (((~i) & 3) * 2)) & 3;
            if (x >= 20 || y >= 18) break;
            attributes[y * 20 + x] = palette;
            if (vertical) { if (++y == 18) { y = 0; if (++x == 20) break; } }
            else { if (++x == 20) { x = 0; if (++y == 18) break; } }
        }
        return;
    }

    if (op == 0x11)
    {
        const uint8_t request = command[1] & 3;
        playerCount = request == 0 ? 1 : (request == 1 ? 2 : (request == 3 ? 4 : 1));
        currentPlayer &= playerCount - 1;
        return;
    }

    if (op == 4)
    {
        const uint8_t count = std::min<uint8_t>(command[1], 18);
        for (uint8_t i = 0; i < count; ++i)
        {
            const uint8_t *block = &command[2 + i * 6];
            const uint8_t control = block[0];
            const uint8_t insidePalette = block[1] & 3;
            const uint8_t middlePalette = (block[1] >> 2) & 3;
            const uint8_t outsidePalette = (block[1] >> 4) & 3;
            const bool changeInside = (control & 1) != 0;
            bool changeEdge = (control & 2) != 0;
            const bool changeOutside = (control & 4) != 0;
            uint8_t edgePalette = middlePalette;
            if (changeInside && !changeEdge && !changeOutside) { changeEdge = true; edgePalette = insidePalette; }
            else if (changeOutside && !changeEdge && !changeInside) { changeEdge = true; edgePalette = outsidePalette; }
            const uint8_t left = block[2] & 0x1F, top = block[3] & 0x1F;
            const uint8_t right = block[4] & 0x1F, bottom = block[5] & 0x1F;
            for (uint8_t y = 0; y < 18; ++y) for (uint8_t x = 0; x < 20; ++x)
            {
                const bool outside = x < left || x > right || y < top || y > bottom;
                const bool inside = x > left && x < right && y > top && y < bottom;
                const bool edge = !outside && !inside;
                if (outside && changeOutside) attributes[y * 20 + x] = outsidePalette;
                else if (inside && changeInside) attributes[y * 20 + x] = insidePalette;
                else if (edge && changeEdge) attributes[y * 20 + x] = edgePalette;
            }
        }
        return;
    }

    if (op == 0x17) maskMode = command[1] & 3;
}

uint32_t SGB::getColor(uint8_t palette, uint8_t color, uint8_t x, uint8_t y)
{
    const size_t index = y * 160 + x;
    if (maskMode == 1) return previousFrame[index];
    uint32_t rgb = maskMode == 2 ? 0x000000 :
        (maskMode == 3 ? palettes[0] : palettes[(palette & 3) * 4 + (color & 3)]);
    previousFrame[index] = rgb;
    return rgb;
}

uint8_t SGB::getAttribute(uint8_t x, uint8_t y) const
{
    return attributes[(std::min<uint8_t>(y / 8, 17) * 20) + std::min<uint8_t>(x / 8, 19)];
}
