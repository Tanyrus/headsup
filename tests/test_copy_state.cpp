#include "copy_state.h"

#include <array>
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

namespace
{
    // A pretransformed vertex as the game's copies carry it: x, y, z, rhw, a color and one texture coordinate.
    struct QuadVertex
    {
        float x, y, z, rhw;
        uint32_t color;
        float u, v;
    };
    std::array<QuadVertex, 4> Quad(float x0, float y0, float x1, float y1)
    {
        return {{{x0, y0, 0, 1, 0, 0, 0}, {x1, y0, 0, 1, 0, 1, 0}, {x0, y1, 0, 1, 0, 0, 1}, {x1, y1, 0, 1, 0, 1, 1}}};
    }
    bool Covers(const std::array<QuadVertex, 4>& quad, float width, float height)
    {
        return CoversTarget(quad.data(), 4, sizeof(QuadVertex), width, height);
    }
}

// From a draw dump of the test client (2026-10-12, a 2560 by 1440 back buffer): the scene's copy is a quad from
// (-0.5, -0.5) to (2559.5, 1439.5), the interface's over it from (0, 0) to (2560, 1440), and the quad after them 16
// pixels square at (329.5, 47.5).
TEST(the_copies_to_the_back_buffer_cover_it_and_smaller_quads_do_not)
{
    CHECK(Covers(Quad(-0.5f, -0.5f, 2559.5f, 1439.5f), 2560.0f, 1440.0f));
    CHECK(Covers(Quad(0.0f, 0.0f, 2560.0f, 1440.0f), 2560.0f, 1440.0f));
    CHECK(!Covers(Quad(329.5f, 47.5f, 345.5f, 63.5f), 2560.0f, 1440.0f));
    CHECK(!Covers(Quad(0.0f, 0.0f, 2560.0f, 720.0f), 2560.0f, 1440.0f));  // the upper half
    CHECK(!Covers(Quad(0.0f, 720.0f, 2560.0f, 1440.0f), 2560.0f, 1440.0f)); // the lower half
    CHECK(!Covers(Quad(0.0f, 0.0f, 1280.0f, 1440.0f), 2560.0f, 1440.0f));   // the left half
    CHECK(!Covers(Quad(0.0f, 0.0f, 1920.0f, 1080.0f), 2560.0f, 1440.0f)); // a smaller image's own full quad
    CHECK(!Covers(Quad(2.0f, 0.0f, 2560.0f, 1440.0f), 2560.0f, 1440.0f)); // two pixels short at the left
    CHECK(!CoversTarget(nullptr, 4, sizeof(QuadVertex), 2560.0f, 1440.0f));
    const auto quad = Quad(0.0f, 0.0f, 2560.0f, 1440.0f);
    CHECK(!CoversTarget(quad.data(), 0, sizeof(QuadVertex), 2560.0f, 1440.0f));
    CHECK(!CoversTarget(quad.data(), 4, 4, 2560.0f, 1440.0f)); // a stride with no room for x and y
    CHECK(!Covers(quad, 0.0f, 0.0f));                         // no back buffer known
}
