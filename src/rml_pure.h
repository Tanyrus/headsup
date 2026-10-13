#pragma once

#include "screen_vertex.h"

#include <RmlUi/Core/Input.h>

#include <cstddef>
#include <cstdint>
#include <optional>

namespace headsup
{
    // RmlUi's vertex (pixel coordinates, premultiplied RGBA bytes, the image's texture coordinates) as Direct3D 8
    // draws it: moved by a translation, at the pixel centers Direct3D samples, its texture coordinates scaled to the
    // image's share (shareU, shareV) of the padded texture it is in.
    ScreenVertex PlacedVertex(float x, float y, uint8_t r, uint8_t g, uint8_t b, uint8_t a, float u, float v,
        float translateX, float translateY, float shareU, float shareV);

    // RmlUi's RGBA bytes as the ARGB words an A8R8G8B8 texture holds.
    void ArgbFromRgba(const uint8_t* rgba, size_t pixels, uint32_t* argb);

    // A straight-alpha ARGB color with each channel multiplied by its alpha, as RmlUi's textures are blended.
    constexpr uint32_t Premultiplied(uint32_t argb)
    {
        constexpr uint32_t kMax = 0xFF;
        const uint32_t alpha    = argb >> 24;
        auto channel            = [&](int shift) { return (((argb >> shift) & kMax) * alpha + kMax / 2) / kMax; };
        return alpha << 24 | channel(16) << 16 | channel(8) << 8 | channel(0);
    }

    struct PixelRect
    {
        int x = 0, y = 0, width = 0, height = 0;
        bool operator==(const PixelRect&) const = default;
    };
    // The part of a rectangle (right and bottom exclusive) inside a width by height target; nothing when no pixel is.
    std::optional<PixelRect> ClampedRect(int left, int top, int right, int bottom, int width, int height);

    // A Windows virtual key as RmlUi names it; KI_UNKNOWN for one it has no name for. Shift, Control and Menu
    // without a side read as their left keys.
    Rml::Input::KeyIdentifier KeyOf(uint32_t virtualKey);
    int ModifiersOf(bool ctrl, bool shift, bool alt, bool capsLock, bool numLock);

    // A wheel message's distance as RmlUi takes it: notches, positive toward the user (down the page), from
    // Windows' delta, positive away.
    float WheelNotches(int delta);
}
