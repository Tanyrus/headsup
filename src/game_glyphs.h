#pragma once

#include "argb.h"
#include "screen_box.h"

#include <cstdint>
#include <unordered_map>

namespace headsup
{
    // D3DPRIMITIVETYPE's values: this file is built without the Direct3D headers.
    enum PrimitiveType : uint32_t
    {
        kPointList = 1,
        kLineList,
        kLineStrip,
        kTriangleList,
        kTriangleStrip,
        kTriangleFan,
    };
    // Every type counts: the caller reads this many vertices from the draw.
    uint32_t VertexCount(uint32_t primitiveType, uint32_t primitiveCount);

    // x, y, z and rhw; the diffuse color, when present, follows.
    constexpr uint32_t kPretransformedPositionBytes = 4 * sizeof(float);

    // The game draws its names inside the scene (0 < z < 1), and HUD text, such as the target bar's copy of a name, at 0.
    constexpr uint32_t kMaxTextVertices = 256;
    bool WorldTextBox(const void* vertices, uint32_t stride, uint32_t count, ScreenBox& box, float& depth);

    constexpr uint32_t kQuadVertices = 4;
    bool UiQuadBox(const void* vertices, uint32_t stride, ScreenBox& box);

    // Draws per texture, of the frame's world text credited to an entity.
    using TextureUse = std::unordered_map<uintptr_t, uint32_t>;
    // The font of the game's names: the texture most of the frame's world letters used, or last when it drew none.
    uintptr_t MostUsedTexture(const TextureUse& use, uintptr_t last);
}
