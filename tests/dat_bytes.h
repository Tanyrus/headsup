#pragma once

#include <array>
#include <cstdint>
#include <cstring>
#include <vector>

// The pieces of a game DAT as the tests build them.
namespace dat
{
    using Bytes = std::vector<uint8_t>;

    inline void Put16(Bytes& out, uint16_t v) { out.insert(out.end(), {static_cast<uint8_t>(v), static_cast<uint8_t>(v >> 8)}); }
    inline void Put32(Bytes& out, uint32_t v)
    {
        for (int i = 0; i < 4; ++i)
            out.push_back(static_cast<uint8_t>(v >> (8 * i)));
    }
    inline void PutText(Bytes& out, const char* text) { out.insert(out.end(), text, text + std::strlen(text)); }
    inline Bytes operator+(Bytes a, const Bytes& b)
    {
        a.insert(a.end(), b.begin(), b.end());
        return a;
    }

    // A DAT chunk: its name, a word of its type (low 7 bits) and its size in 16-byte units (the next 19), 8 bytes
    // unused, then the body padded to 16 bytes.
    inline Bytes Chunk(const char* name, uint32_t type, Bytes body)
    {
        body.resize((body.size() + 15) / 16 * 16);
        Bytes out(name, name + 4);
        Put32(out, static_cast<uint32_t>((16 + body.size()) / 16) << 7 | type);
        out.resize(16);
        return out + body;
    }

    // A picture chunk's body as the game's DATs have it: a flag, a 16-byte name, a BITMAPINFOHEADER, the four-letter
    // code stored backwards, the data's size and a dword, then the blocks.
    inline Bytes Picture(const char* name, int32_t width, int32_t height, const char* code, const Bytes& blocks, uint8_t flag = 0xA1)
    {
        Bytes out(name, name + std::strlen(name));
        out.insert(out.begin(), flag);
        Put32(out, 40);
        Put32(out, static_cast<uint32_t>(width));
        Put32(out, static_cast<uint32_t>(height));
        Put16(out, 1);
        Put16(out, 32);
        out.resize(out.size() + 24);
        for (int i = 3; i >= 0; --i)
            out.push_back(static_cast<uint8_t>(code[i]));
        Put32(out, static_cast<uint32_t>(blocks.size()));
        Put32(out, 0x400);
        return out + blocks;
    }

    // Direct3D's DXT1 block of 4 x 4 pixels, the first pixel in the lowest bits: two 565 colors and a 2-bit pick a pixel.
    inline Bytes ColorBlock(uint16_t c0, uint16_t c1, const std::array<uint8_t, 16>& picks)
    {
        Bytes out;
        Put16(out, c0);
        Put16(out, c1);
        uint32_t bits = 0;
        for (int k = 0; k < 16; ++k)
            bits |= static_cast<uint32_t>(picks[static_cast<size_t>(k)]) << (2 * k);
        Put32(out, bits);
        return out;
    }
}
