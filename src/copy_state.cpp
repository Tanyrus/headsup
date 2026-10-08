#include "copy_state.h"

namespace headsup
{
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
