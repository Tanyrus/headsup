#include "nameplate.h"
#include "test.h"

#include <cmath>
#include <vector>

using namespace aggroglow;

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

    // A glyph quad as a 4-vertex triangle strip.
    std::vector<Vertex> Quad(float x0, float y0, float x1, float y1, float z)
    {
        return {{x0, y0, z, 1.0f, 0xFFFFFFFF, 0, 0}, {x1, y0, z, 1.0f, 0xFFFFFFFF, 1, 0},
            {x0, y1, z, 1.0f, 0xFFFFFFFF, 0, 1}, {x1, y1, z, 1.0f, 0xFFFFFFFF, 1, 1}};
    }

    bool Near(float a, float b)
    {
        return std::fabs(a - b) < 1e-3f;
    }
}

TEST(vertex_counts_by_primitive_type)
{
    CHECK_EQ(VertexCount(5, 2), 4u); // triangle strip: one glyph quad
    CHECK_EQ(VertexCount(4, 2), 6u);
    CHECK_EQ(VertexCount(6, 3), 5u);
    CHECK_EQ(VertexCount(2, 3), 6u);
    CHECK_EQ(VertexCount(3, 3), 4u);
    CHECK_EQ(VertexCount(1, 7), 7u);
    CHECK_EQ(VertexCount(0, 2), 0u);
}

TEST(nameplate_glyph_gives_its_box)
{
    // A Beach Pugil glyph from the 2026-10-05 draw dump (3840x2160 render target, depth 0.997).
    const auto quad = Quad(389.016f, 601.077f, 396.38f, 610.281f, 0.997f);
    ScreenBox box;
    CHECK(WorldTextBox(quad.data(), sizeof(Vertex), 4, box));
    CHECK(box.valid);
    CHECK(Near(box.minX, 389.016f) && Near(box.maxX, 396.38f));
    CHECK(Near(box.minY, 601.077f) && Near(box.maxY, 610.281f));
}

TEST(hud_text_at_depth_zero_is_rejected)
{
    // The target bar's copy of the same mob's name, from the same dump.
    const auto quad = Quad(45.5f, 881.5f, 50.5f, 891.5f, 0.0f);
    ScreenBox box;
    CHECK(!WorldTextBox(quad.data(), sizeof(Vertex), 4, box));
    CHECK(!box.valid);
}

TEST(any_vertex_outside_the_scene_depth_rejects_the_draw)
{
    auto quad = Quad(10, 10, 20, 20, 0.5f);
    quad[3].z = 1.0f;
    ScreenBox box;
    CHECK(!WorldTextBox(quad.data(), sizeof(Vertex), 4, box));
    quad[3].z = std::nanf("");
    CHECK(!WorldTextBox(quad.data(), sizeof(Vertex), 4, box));
    quad[3].z = 0.5f;
    quad[3].x = INFINITY;
    CHECK(!WorldTextBox(quad.data(), sizeof(Vertex), 4, box));
}

TEST(unusable_draws_are_rejected)
{
    const auto quad = Quad(10, 10, 20, 20, 0.5f);
    ScreenBox box;
    CHECK(!WorldTextBox(nullptr, sizeof(Vertex), 4, box));
    CHECK(!WorldTextBox(quad.data(), 12, 4, box));
    CHECK(!WorldTextBox(quad.data(), sizeof(Vertex), 0, box));
    std::vector<Vertex> many(kMaxTextVertices + 1, quad[0]);
    CHECK(!WorldTextBox(many.data(), sizeof(Vertex), kMaxTextVertices + 1, box));
}

TEST(glyph_boxes_merge_into_the_nameplate)
{
    ScreenBox plate, glyph;
    const auto first  = Quad(381.653f, 601.077f, 389.016f, 610.281f, 0.997f);
    const auto second = Quad(389.016f, 601.077f, 396.38f, 612.0f, 0.997f);
    CHECK(WorldTextBox(first.data(), sizeof(Vertex), 4, glyph));
    plate.Add(glyph);
    CHECK(WorldTextBox(second.data(), sizeof(Vertex), 4, glyph));
    plate.Add(glyph);
    plate.Add(ScreenBox{}); // an invalid box changes nothing
    CHECK(Near(plate.minX, 381.653f) && Near(plate.maxX, 396.38f));
    CHECK(Near(plate.minY, 601.077f) && Near(plate.maxY, 612.0f));
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

TEST(label_is_centered_above_the_box)
{
    ScreenBox box;
    box.Add(100.0f, 50.0f);
    box.Add(140.0f, 60.0f);
    float x = 0.0f, y = 0.0f;
    PlaceAbove(box, 30.0f, 14.0f, 2.0f, x, y);
    CHECK(Near(x, 105.0f));
    CHECK(Near(y, 34.0f));
}

namespace
{
    ScreenBox Glyph(float x0, float y0, float x1, float y1)
    {
        ScreenBox b;
        b.Add(x0, y0);
        b.Add(x1, y1);
        return b;
    }

    // "Brutal Sheep"-like nameplate: a lead quad and 11 letters, 7 px wide, 10 px tall, at y 200.
    std::vector<ScreenBox> Name(float x)
    {
        std::vector<ScreenBox> glyphs{Glyph(x - 7.0f, 200.0f, x, 210.0f)};
        for (int i = 0; i < 11; ++i)
        {
            const float left = x + i * 7.0f + (i >= 6 ? 4.0f : 0.0f); // a space after the sixth letter
            glyphs.push_back(Glyph(left, 200.0f, left + 7.0f, 210.0f));
        }
        return glyphs;
    }
}

TEST(nameplate_box_is_the_whole_name)
{
    const ScreenBox box = NameplateFromGlyphs(Name(1000.0f));
    CHECK(box.valid);
    CHECK(Near(box.minX, 993.0f) && Near(box.maxX, 1081.0f));
    CHECK(Near(box.minY, 200.0f) && Near(box.maxY, 210.0f));
}

TEST(a_stray_glyph_from_another_nameplate_is_ignored)
{
    // One letter of a different name, attributed to this mob on some frames by a stale stack pointer.
    auto glyphs = Name(1000.0f);
    glyphs.push_back(Glyph(400.0f, 520.0f, 407.0f, 530.0f));
    const ScreenBox far = NameplateFromGlyphs(glyphs);
    CHECK(Near(far.minX, 993.0f) && Near(far.maxX, 1081.0f) && Near(far.maxY, 210.0f));

    // Same line but well past the end of the name.
    glyphs.back() = Glyph(1200.0f, 200.0f, 1207.0f, 210.0f);
    const ScreenBox sameLine = NameplateFromGlyphs(glyphs);
    CHECK(Near(sameLine.minX, 993.0f) && Near(sameLine.maxX, 1081.0f));
}

TEST(no_glyphs_no_box)
{
    CHECK(!NameplateFromGlyphs({}).valid);
}

TEST(label_shows_for_a_steady_on_screen_nameplate_of_a_drawn_mob)
{
    const ScreenBox plate = Glyph(1000.0f, 200.0f, 1080.0f, 210.0f);
    CHECK(LabelVisible(&plate, 18, kStableFrames, 2560.0f, 1440.0f));
}

TEST(label_hides_when_the_camera_cannot_see_the_mob)
{
    const ScreenBox plate = Glyph(1000.0f, 200.0f, 1080.0f, 210.0f);
    CHECK(!LabelVisible(nullptr, 18, kStableFrames, 2560.0f, 1440.0f));
    CHECK(!LabelVisible(&plate, 0, kStableFrames, 2560.0f, 1440.0f));                // body not drawn
    CHECK(!LabelVisible(&plate, 18, kStableFrames - 1, 2560.0f, 1440.0f));           // nameplate just appeared
    const ScreenBox invalid;
    CHECK(!LabelVisible(&invalid, 18, kStableFrames, 2560.0f, 1440.0f));
}

TEST(label_hides_when_the_nameplate_is_not_fully_on_screen)
{
    CHECK(!LabelVisible(&static_cast<const ScreenBox&>(Glyph(-5.0f, 200.0f, 60.0f, 210.0f)), 18, kStableFrames, 2560.0f, 1440.0f));
    CHECK(!LabelVisible(&static_cast<const ScreenBox&>(Glyph(2500.0f, 200.0f, 2570.0f, 210.0f)), 18, kStableFrames, 2560.0f, 1440.0f));
    CHECK(!LabelVisible(&static_cast<const ScreenBox&>(Glyph(1000.0f, -3.0f, 1080.0f, 7.0f)), 18, kStableFrames, 2560.0f, 1440.0f));
    CHECK(!LabelVisible(&static_cast<const ScreenBox&>(Glyph(1000.0f, 1435.0f, 1080.0f, 1445.0f)), 18, kStableFrames, 2560.0f, 1440.0f));
}

TEST(a_huge_in_scene_quad_is_not_a_nameplate)
{
    // From the 2026-10-06 capture: every fourth frame a ~2000 px quad is attributed to whichever mob is on the stack.
    const ScreenBox huge = Glyph(667.0f, 401.0f, 2731.0f, 563.0f);
    CHECK(!NameplateFromGlyphs({huge}).valid);

    auto glyphs = Name(1000.0f);
    glyphs.push_back(huge);
    const ScreenBox box = NameplateFromGlyphs(glyphs);
    CHECK(Near(box.minX, 993.0f) && Near(box.maxX, 1081.0f) && Near(box.maxY, 210.0f));
}

TEST(fewer_than_three_glyphs_are_not_a_nameplate)
{
    CHECK(!NameplateFromGlyphs({Glyph(10, 10, 17, 20), Glyph(17, 10, 24, 20)}).valid);
    CHECK(NameplateFromGlyphs({Glyph(10, 10, 17, 20), Glyph(17, 10, 24, 20), Glyph(24, 10, 31, 20)}).valid);
}
