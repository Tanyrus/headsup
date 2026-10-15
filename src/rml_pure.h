#pragma once

#include "screen_vertex.h"

#include <RmlUi/Core/Input.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace headsup
{
    // RmlUi's vertex (pixel coordinates, premultiplied RGBA bytes, the image's texture coordinates) as Direct3D 8
    // draws it: moved by a translation, at the pixel centers Direct3D samples, its texture coordinates scaled to the
    // image's share (shareU, shareV) of the padded texture it is in.
    ScreenVertex PlacedVertex(float x, float y, uint8_t r, uint8_t g, uint8_t b, uint8_t a, float u, float v,
        float translateX, float translateY, float shareU, float shareV);

    // A layer (a picture of a whole element, as a filter needs) is laid over the target below it as one quad: its
    // vertex color scales the premultiplied picture by the opacity, the product of the opacity filters asked for.
    uint32_t LayerTint(float opacity);
    constexpr const char* kOpacityFilter = "opacity"; // RCSS's filter, the one filter the renderer draws
    // The depth drawing goes into for a stack of pushed layers (made: false for one Direct3D refused) at depth:
    // the highest made at or under it, 0 being the game's own target.
    size_t DrawnDepth(const std::vector<bool>& made, size_t depth);
    // Counts renders with no layer drawn into (pushed resets it): true once the layers have been idle for
    // kIdleRendersBeforeRelease of them, two a frame (the panel's and the overlay's), so a window closed or back at
    // full opacity keeps no video memory for long.
    constexpr int kIdleRendersBeforeRelease = 120;
    bool ReleaseIdleLayers(int& idleRenders, bool pushed);
    // The two triangles over a width by height target, the layer's picture its share (shareU, shareV) of the padded
    // texture it is drawn into.
    std::array<ScreenVertex, 6> LayerQuad(int width, int height, float shareU, float shareV, uint32_t tint);

    // A point as a transform moves it: the matrix is RmlUi's, sixteen floats column by column, and the point is
    // divided by the depth it comes out at, as a perspective needs.
    void Transformed(const float* matrix, float& x, float& y);

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

    // A rectangle by its edges in whole pixels, right and bottom exclusive; it holds no pixel when they cross.
    struct PixelEdges
    {
        int left = 0, top = 0, right = 0, bottom = 0;
        bool operator==(const PixelEdges&) const = default;
    };
    // The box around a clip mask's vertices as they are drawn: moved by the translation, then by the transform when
    // there is one. Direct3D 8 gives no stencil to count on, so a mask clips as this rectangle: a moved or scaled
    // box exactly, a turned one by the box around it, and rounded corners not at all.
    PixelEdges MaskEdges(const ScreenVertex* vertices, size_t count, float translateX, float translateY,
        const float* transform);
    // What two rectangles both cover.
    PixelEdges Shared(const PixelEdges& a, const PixelEdges& b);

    // A Windows virtual key as RmlUi names it; KI_UNKNOWN for one it has no name for. Shift, Control and Menu
    // without a side read as their left keys.
    Rml::Input::KeyIdentifier KeyOf(uint32_t virtualKey);
    int ModifiersOf(bool ctrl, bool shift, bool alt, bool capsLock, bool numLock);

    // A wheel message's distance as RmlUi takes it: notches, positive toward the user (down the page), from
    // Windows' delta, positive away.
    float WheelNotches(int delta);
}
