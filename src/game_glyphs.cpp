#include "game_glyphs.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>

namespace headsup
{
    namespace
    {
        template <typename DepthOk>
        bool BoxOf(const void* vertices, uint32_t stride, uint32_t count, ScreenBox& box, float& depth, DepthOk depthOk)
        {
            box   = ScreenBox{};
            depth = 0.0f;
            if (vertices == nullptr || stride < kPretransformedPositionBytes) return false;
            const auto* bytes = static_cast<const uint8_t*>(vertices);
            for (uint32_t i = 0; i < count; ++i)
            {
                float xyz[3];
                std::memcpy(xyz, bytes + static_cast<size_t>(i) * stride, sizeof(xyz));
                if (!depthOk(xyz[2]) || !std::isfinite(xyz[0]) || !std::isfinite(xyz[1])) return false;
                box.Add(xyz[0], xyz[1]);
                depth = std::max(depth, xyz[2]);
            }
            return true;
        }
    }

    uint32_t VertexCount(uint32_t primitiveType, uint32_t primitiveCount)
    {
        switch (primitiveType)
        {
            case kPointList: return primitiveCount;
            case kLineList: return primitiveCount * 2;
            case kLineStrip: return primitiveCount + 1;
            case kTriangleList: return primitiveCount * 3;
            case kTriangleStrip:
            case kTriangleFan: return primitiveCount + 2;
            default: return 0;
        }
    }

    bool WorldTextBox(const void* vertices, uint32_t stride, uint32_t count, ScreenBox& box, float& depth)
    {
        if (count == 0 || count > kMaxTextVertices) return false;
        return BoxOf(vertices, stride, count, box, depth, [](float z) { return z > 0.0f && z < 1.0f; });
    }

    bool UiQuadBox(const void* vertices, uint32_t stride, ScreenBox& box)
    {
        float depth = 0.0f;
        return BoxOf(vertices, stride, kQuadVertices, box, depth, [](float z) { return z == 0.0f; });
    }

    uintptr_t MostUsedTexture(const TextureUse& use, uintptr_t last)
    {
        uint32_t most = 0;
        for (const auto& [texture, count] : use)
            if (count > most) last = texture, most = count;
        return last;
    }
}
