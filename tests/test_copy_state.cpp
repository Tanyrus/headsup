#include "copy_state.h"
#include "test.h"

using namespace headsup;

namespace
{
    // The scene copy as the tester's Windows drawdump recorded it (2026-10-08): the scene's texture over the whole
    // 1920x1080 back buffer, unlit and unblended, with the game's usual pretransformed vertex format.
    CopyState SceneCopy()
    {
        CopyState state;
        state.texture        = 0x18CE3640;
        state.colorOp        = 4; // D3DTOP_MODULATE
        state.colorArg1      = 2; // D3DTA_TEXTURE
        state.colorArg2      = 0; // D3DTA_DIFFUSE
        state.alphaOp        = 4;
        state.vertexShader   = 0x144;
        state.viewportWidth  = 1920;
        state.viewportHeight = 1080;
        state.srcBlend       = 2;
        state.destBlend      = 1;
        return state;
    }
}

TEST(nothing_is_reported_when_every_state_came_back)
{
    CHECK_EQ(StatesNotRestored(SceneCopy(), SceneCopy()), "");
}

TEST(a_texture_left_unbound_is_reported)
{
    // HeadsUp unbinds its own textures when it is done; the copy then draws its gray vertex color over the screen.
    CopyState after = SceneCopy();
    after.texture   = 0;
    CHECK_EQ(StatesNotRestored(SceneCopy(), after), "texture 0");
}

TEST(every_state_that_did_not_come_back_is_named)
{
    CopyState after       = SceneCopy();
    after.colorOp         = 3; // D3DTOP_SELECTARG2
    after.colorArg1       = 1;
    after.colorArg2       = 3;
    after.alphaOp         = 1;
    after.vertexShader    = 0x142;
    after.viewportX       = 64;
    after.viewportY       = 32;
    after.viewportWidth   = 2048;
    after.viewportHeight  = 2048;
    after.zEnable         = 1;
    after.alphaBlend      = 1;
    after.srcBlend        = 5;
    after.destBlend       = 6;
    CHECK_EQ(StatesNotRestored(SceneCopy(), after),
        "color op, color argument 1, color argument 2, alpha op, vertex shader, viewport, depth test, blending, "
        "source blend, destination blend");
}

TEST(a_viewport_moved_or_resized_is_reported_once)
{
    CopyState right = SceneCopy();
    right.viewportX = 1;
    CHECK_EQ(StatesNotRestored(SceneCopy(), right), "viewport");
    CopyState down = SceneCopy();
    down.viewportY = 1;
    CHECK_EQ(StatesNotRestored(SceneCopy(), down), "viewport");
    CopyState wider     = SceneCopy();
    wider.viewportWidth = 2048;
    CHECK_EQ(StatesNotRestored(SceneCopy(), wider), "viewport");
    CopyState taller      = SceneCopy();
    taller.viewportHeight = 2048;
    CHECK_EQ(StatesNotRestored(SceneCopy(), taller), "viewport");
}
