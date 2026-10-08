#pragma once

#include <cstdint>

namespace headsup
{
    constexpr uint32_t kWhite = 0xFFFFFFFF;
    constexpr int kRedShift = 16, kGreenShift = 8, kBlueShift = 0, kAlphaShift = 24; // in a D3DCOLOR (A8R8G8B8)

    constexpr uint32_t Argb(uint8_t a, uint8_t r, uint8_t g, uint8_t b)
    {
        return uint32_t{a} << kAlphaShift | uint32_t{r} << kRedShift | uint32_t{g} << kGreenShift | uint32_t{b} << kBlueShift;
    }

    constexpr uint32_t Opaque(uint8_t r, uint8_t g, uint8_t b) { return Argb(0xFF, r, g, b); }

    constexpr uint8_t Channel(uint32_t argb, int shift) { return static_cast<uint8_t>(argb >> shift); }

    // A vertex color as drawn with a modulate texture stage that multiplies by modulateScale, made opaque.
    constexpr uint32_t ShownColor(uint32_t diffuse, uint32_t modulateScale)
    {
        auto channel = [&](int shift) {
            const uint32_t scaled = Channel(diffuse, shift) * modulateScale;
            return static_cast<uint8_t>(scaled < 0xFF ? scaled : 0xFF);
        };
        return Opaque(channel(kRedShift), channel(kGreenShift), channel(kBlueShift));
    }
}
