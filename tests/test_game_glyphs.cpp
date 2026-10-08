#include "game_glyphs.h"
#include "boxes.h"
#include "test.h"

#include <cmath>
#include <vector>

using namespace headsup;
using test::Near;

namespace
{
    // XYZRHW | DIFFUSE | TEX1, 28 bytes: the layout of the game's nameplate glyphs.
    struct Vertex
    {
        float x, y, z, rhw;
        uint32_t color;
        float u, v;
    };
    static_assert(sizeof(Vertex) == 28);

    std::vector<Vertex> Quad(float x0, float y0, float x1, float y1, float z)
    {
        return {{x0, y0, z, 1.0f, 0xFFFFFFFF, 0, 0}, {x1, y0, z, 1.0f, 0xFFFFFFFF, 1, 0},
            {x0, y1, z, 1.0f, 0xFFFFFFFF, 0, 1}, {x1, y1, z, 1.0f, 0xFFFFFFFF, 1, 1}};
    }
}

TEST(vertex_counts_by_primitive_type)
{
    CHECK_EQ(VertexCount(kTriangleStrip, 2), 4u); // one glyph quad
    CHECK_EQ(VertexCount(kTriangleList, 2), 6u);
    CHECK_EQ(VertexCount(kTriangleFan, 3), 5u);
    CHECK_EQ(VertexCount(kLineList, 3), 6u);
    CHECK_EQ(VertexCount(kLineStrip, 3), 4u);
    CHECK_EQ(VertexCount(kPointList, 7), 7u);
    CHECK_EQ(VertexCount(0, 2), 0u);
}

TEST(nameplate_glyph_gives_its_box)
{
    // A nameplate letter: drawn inside the scene, at depth 0.997.
    auto quad = Quad(389.016f, 601.077f, 396.38f, 610.281f, 0.997f);
    quad[2].z = 0.998f;
    ScreenBox box;
    float depth = 0.0f;
    CHECK(WorldTextBox(quad.data(), sizeof(Vertex), 4, box, depth));
    CHECK(box.valid);
    CHECK(Near(box.minX, 389.016f) && Near(box.maxX, 396.38f));
    CHECK(Near(box.minY, 601.077f) && Near(box.maxY, 610.281f));
    CHECK(Near(depth, 0.998f)); // the farthest corner
}

TEST(hud_text_at_depth_zero_is_rejected)
{
    // The target bar's copy of a mob's name: HUD text at depth 0.
    const auto quad = Quad(45.5f, 881.5f, 50.5f, 891.5f, 0.0f);
    ScreenBox box;
    float depth = 0.0f;
    CHECK(!WorldTextBox(quad.data(), sizeof(Vertex), 4, box, depth));
    CHECK(!box.valid);
}

TEST(any_vertex_outside_the_scene_depth_rejects_the_draw)
{
    auto quad = Quad(10, 10, 20, 20, 0.5f);
    quad[3].z = 1.0f;
    ScreenBox box;
    float depth = 0.0f;
    CHECK(!WorldTextBox(quad.data(), sizeof(Vertex), 4, box, depth));
    quad[3].z = std::nanf("");
    CHECK(!WorldTextBox(quad.data(), sizeof(Vertex), 4, box, depth));
    quad[3].z = 0.5f;
    quad[3].x = INFINITY;
    CHECK(!WorldTextBox(quad.data(), sizeof(Vertex), 4, box, depth));
}

TEST(a_ui_quad_gives_its_box)
{
    ScreenBox box;
    CHECK(UiQuadBox(Quad(1068.5f, 162.5f, 1088.5f, 194.5f, 0.0f).data(), sizeof(Vertex), box)); // the game's cursor
    CHECK(Near(box.minX, 1068.5f) && Near(box.maxY, 194.5f));
    CHECK(!UiQuadBox(Quad(10, 10, 20, 20, 0.5f).data(), sizeof(Vertex), box)); // in the scene, not the UI
    CHECK(!UiQuadBox(nullptr, sizeof(Vertex), box));
    CHECK(!UiQuadBox(Quad(10, 10, 20, 20, 0.0f).data(), 12, box));
}

TEST(unusable_draws_are_rejected)
{
    const auto quad = Quad(10, 10, 20, 20, 0.5f);
    ScreenBox box;
    float depth = 0.0f;
    CHECK(!WorldTextBox(nullptr, sizeof(Vertex), 4, box, depth));
    CHECK(!WorldTextBox(quad.data(), 12, 4, box, depth));
    CHECK(!WorldTextBox(quad.data(), sizeof(Vertex), 0, box, depth));
    std::vector<Vertex> many(kMaxTextVertices + 1, quad[0]);
    CHECK(!WorldTextBox(many.data(), sizeof(Vertex), kMaxTextVertices + 1, box, depth));
}

TEST(render_target_box_scales_to_the_back_buffer)
{
    ScreenBox box;
    box.Add(3840.0f * 0.5f, 2160.0f * 0.25f);
    box.Add(3840.0f * 0.75f, 2160.0f * 0.5f);
    const ScreenBox scaled = box.Scaled(2560.0f / 3840.0f, 1440.0f / 2160.0f);
    CHECK(Near(scaled.minX, 1280.0f) && Near(scaled.maxX, 1920.0f));
    CHECK(Near(scaled.minY, 360.0f) && Near(scaled.maxY, 720.0f));
}

TEST(a_letters_shown_color_applies_the_modulate_scale_and_is_opaque)
{
    CHECK_EQ(ShownColor(0x80604020u, 2), 0xFFC08040u);
    CHECK_EQ(ShownColor(0x80808040u, 2), 0xFFFFFF80u); // 0x80 is full intensity
    CHECK_EQ(ShownColor(0x80402010u, 4), 0xFFFF8040u);
    CHECK_EQ(ShownColor(0x80FFFF80u, 1), 0xFFFFFF80u);
}

TEST(the_name_font_is_the_texture_most_world_letters_used)
{
    // The icon strip, a letter's lead quad and the letters themselves can all be credited to names.
    const TextureUse use{{0x1F243DD0, 41}, {0x14F59748, 3}, {0x0F11E200, 9}};
    CHECK_EQ(MostUsedTexture(use, 0x0F11E200), uintptr_t{0x1F243DD0});
}

TEST(a_frame_without_world_letters_keeps_the_font_it_had)
{
    // Every name HeadsUp's: the game draws none of their letters.
    CHECK_EQ(MostUsedTexture(TextureUse{}, 0x1F243DD0), uintptr_t{0x1F243DD0});
}
