#pragma once

#include "argb.h"

#include <cstdint>

namespace headsup
{
    // Opaque: outline copies take alpha from the mob's texture, where it shapes hair and cloth cut-outs.
    struct Color
    {
        float v[3]; // r, g, b in 0-1
        bool operator==(const Color&) const = default;
    };
    // From 0xRRGGBB, exactly: ToArgb gives back the same bytes.
    constexpr Color ColorFromRgb(uint32_t rgb)
    {
        constexpr float kFull = 255.0f;
        return {{Channel(rgb, kRedShift) / kFull, Channel(rgb, kGreenShift) / kFull, Channel(rgb, kBlueShift) / kFull}};
    }
    // Phoenix's royal red, the accent of its own theme (phoenix-platform's --phoenix-accent).
    constexpr uint32_t kPhoenixRed = 0xC55151;
}
