#pragma once

#include <cstdint>

namespace headsup
{
    // A pretransformed vertex as Direct3D 8 draws it with the vertex format XYZRHW | DIFFUSE | TEX1.
    struct ScreenVertex
    {
        float x, y, z, rhw;
        uint32_t color;
        float u, v;
    };
    constexpr float kPixelCenter = 0.5f; // Direct3D 8 samples pixel centers at half-pixel offsets
}
