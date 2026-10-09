#include "argb.h"
#include "boxes.h"
#include "game_glyphs.h"
#include "test.h"

using namespace headsup;
using test::Near;

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
