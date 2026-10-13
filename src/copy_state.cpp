#include "copy_state.h"

#include <algorithm>
#include <cstring>

namespace headsup
{
    bool CoversTarget(const void* vertices, uint32_t count, uint32_t stride, float width, float height)
    {
        constexpr float kEdgeSlack = 1.0f; // the scene's copy starts half a pixel outside
        float xy[2];
        if (vertices == nullptr || count == 0 || stride < sizeof(xy) || width <= 0.0f || height <= 0.0f) return false;
        float left = width, top = height, right = 0.0f, bottom = 0.0f;
        for (uint32_t i = 0; i < count; ++i)
        {
            std::memcpy(xy, static_cast<const uint8_t*>(vertices) + static_cast<size_t>(i) * stride, sizeof(xy));
            left   = std::min(left, xy[0]);
            right  = std::max(right, xy[0]);
            top    = std::min(top, xy[1]);
            bottom = std::max(bottom, xy[1]);
        }
        return left <= kEdgeSlack && top <= kEdgeSlack && right >= width - kEdgeSlack && bottom >= height - kEdgeSlack;
    }

    std::string StatesNotRestored(const CopyState& before, const CopyState& after)
    {
        std::string lost;
        auto check = [&](bool same, const char* name) {
            if (same) return;
            if (!lost.empty()) lost += ", ";
            lost += name;
        };
        check(before.texture == after.texture, "texture 0");
        check(before.colorOp == after.colorOp, "color op");
        check(before.colorArg1 == after.colorArg1, "color argument 1");
        check(before.colorArg2 == after.colorArg2, "color argument 2");
        check(before.alphaOp == after.alphaOp, "alpha op");
        check(before.vertexShader == after.vertexShader, "vertex shader");
        check(before.viewportX == after.viewportX && before.viewportY == after.viewportY &&
                  before.viewportWidth == after.viewportWidth && before.viewportHeight == after.viewportHeight,
            "viewport");
        check(before.zEnable == after.zEnable, "depth test");
        check(before.alphaBlend == after.alphaBlend, "blending");
        check(before.srcBlend == after.srcBlend, "source blend");
        check(before.destBlend == after.destBlend, "destination blend");
        return lost;
    }
}
