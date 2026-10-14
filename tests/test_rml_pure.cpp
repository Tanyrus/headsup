#include "rml_pure.h"
#include "test.h"

#include <algorithm>
#include <vector>

using namespace headsup;
using Rml::Input::KeyIdentifier;

TEST(a_vertex_is_moved_to_the_pixel_center_with_its_texture_share)
{
    // A 2x4 image padded into an 4x16 texture takes half its width and a quarter of its height.
    const ScreenVertex v = PlacedVertex(10.0f, 20.0f, 255, 128, 0, 255, 1.0f, 1.0f, 5.0f, 5.0f, 0.5f, 0.25f);
    CHECK_EQ(v.x, 14.5f);
    CHECK_EQ(v.y, 24.5f);
    CHECK_EQ(v.z, 0.0f);
    CHECK_EQ(v.rhw, 1.0f);
    CHECK_EQ(v.color, 0xFFFF8000u);
    CHECK_EQ(v.u, 0.5f);
    CHECK_EQ(v.v, 0.25f);
}

TEST(a_layer_is_laid_over_the_target_at_its_opacity_in_premultiplied_color)
{
    // Every channel of a premultiplied picture scales with the opacity, its alpha too.
    CHECK_EQ(LayerTint(1.0f), 0xFFFFFFFFu);
    CHECK_EQ(LayerTint(0.5f), 0x80808080u);
    CHECK_EQ(LayerTint(0.0f), 0u);
    CHECK_EQ(LayerTint(2.0f), 0xFFFFFFFFu);
    const float opacities[] = {0.5f, 0.5f};
    CHECK_EQ(FilterOpacity(opacities), 0.25f);
    CHECK_EQ(FilterOpacity({}), 1.0f);
    // Two triangles over the whole target, their texture coordinates the target's share of the padded layer, each
    // pixel's center on its texel's.
    const auto quad = LayerQuad(2560, 1440, 0.625f, 0.703125f, 0x80808080u);
    float left = 1e9f, top = 1e9f, right = -1e9f, bottom = -1e9f, uMax = 0.0f, vMax = 0.0f;
    for (const ScreenVertex& v : quad)
    {
        left   = std::min(left, v.x);
        top    = std::min(top, v.y);
        right  = std::max(right, v.x);
        bottom = std::max(bottom, v.y);
        uMax   = std::max(uMax, v.u);
        vMax   = std::max(vMax, v.v);
        CHECK_EQ(v.color, 0x80808080u);
    }
    CHECK_EQ(left, -kPixelCenter);
    CHECK_EQ(top, -kPixelCenter);
    CHECK_EQ(right, 2560.0f - kPixelCenter);
    CHECK_EQ(bottom, 1440.0f - kPixelCenter);
    CHECK_EQ(uMax, 0.625f);
    CHECK_EQ(vMax, 0.703125f);
}

TEST(rgba_bytes_become_argb_words)
{
    const uint8_t rgba[8] = {0x11, 0x22, 0x33, 0x44, 0xFF, 0x00, 0x80, 0x80};
    uint32_t argb[2]      = {};
    ArgbFromRgba(rgba, 2, argb);
    CHECK_EQ(argb[0], 0x44112233u);
    CHECK_EQ(argb[1], 0x80FF0080u);
}

TEST(premultiplying_scales_the_channels_by_the_alpha)
{
    // Half alpha: 255 to 128, 128 to 64 (64.25 rounded), 0 stays.
    CHECK_EQ(Premultiplied(0x80FF8000u), 0x80804000u);
    CHECK_EQ(Premultiplied(0xFF123456u), 0xFF123456u);
    CHECK_EQ(Premultiplied(0x00FFFFFFu), 0x00000000u);
}

TEST(a_scissor_rectangle_is_kept_inside_the_target)
{
    CHECK((ClampedRect(-10, -10, 100, 50, 80, 40) == PixelRect{0, 0, 80, 40}));
    CHECK((ClampedRect(10, 10, 20, 20, 80, 40) == PixelRect{10, 10, 10, 10}));
    CHECK(!ClampedRect(100, 0, 120, 10, 80, 40));
    CHECK(!ClampedRect(10, 10, 10, 20, 80, 40));
}

TEST(virtual_keys_get_rmlui_names)
{
    CHECK_EQ(KeyOf('A'), KeyIdentifier::KI_A);
    CHECK_EQ(KeyOf('Z'), KeyIdentifier::KI_Z);
    CHECK_EQ(KeyOf('7'), KeyIdentifier::KI_7);
    CHECK_EQ(KeyOf(0x0D), KeyIdentifier::KI_RETURN);
    CHECK_EQ(KeyOf(0x74), KeyIdentifier::KI_F5);
    CHECK_EQ(KeyOf(0x87), KeyIdentifier::KI_F24);
    CHECK_EQ(KeyOf(0x25), KeyIdentifier::KI_LEFT);
    CHECK_EQ(KeyOf(0x63), KeyIdentifier::KI_NUMPAD3);
    CHECK_EQ(KeyOf(0xBA), KeyIdentifier::KI_OEM_1);
    CHECK_EQ(KeyOf(0x10), KeyIdentifier::KI_LSHIFT);
    CHECK_EQ(KeyOf(0xA1), KeyIdentifier::KI_RSHIFT);
    CHECK_EQ(KeyOf(0x2E), KeyIdentifier::KI_DELETE);
    CHECK_EQ(KeyOf(0x07), KeyIdentifier::KI_UNKNOWN);
    CHECK_EQ(KeyOf(0x100), KeyIdentifier::KI_UNKNOWN);
}

TEST(modifiers_are_rmluis_bits)
{
    CHECK_EQ(ModifiersOf(true, false, true, false, false), Rml::Input::KM_CTRL | Rml::Input::KM_ALT);
    CHECK_EQ(ModifiersOf(false, true, false, true, true),
        Rml::Input::KM_SHIFT | Rml::Input::KM_CAPSLOCK | Rml::Input::KM_NUMLOCK);
    CHECK_EQ(ModifiersOf(false, false, false, false, false), 0);
}

TEST(wheel_deltas_become_notches_toward_the_user)
{
    CHECK_EQ(WheelNotches(120), -1.0f);
    CHECK_EQ(WheelNotches(-240), 2.0f);
    CHECK_EQ(WheelNotches(60), -0.5f);
}

TEST(a_transform_moves_a_point_and_divides_by_its_depth)
{
    // Column by column: twice as wide, three times as tall, moved 10 right and 20 down.
    const float scaleAndMove[16] = {2, 0, 0, 0, 0, 3, 0, 0, 0, 0, 1, 0, 10, 20, 0, 1};
    float x = 5.0f, y = 7.0f;
    Transformed(scaleAndMove, x, y);
    CHECK_EQ(x, 20.0f);
    CHECK_EQ(y, 41.0f);
    // A quarter turn: (1, 0) goes to (0, 1).
    const float turn[16] = {0, 1, 0, 0, -1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    x = 1.0f, y = 0.0f;
    Transformed(turn, x, y);
    CHECK_EQ(x, 0.0f);
    CHECK_EQ(y, 1.0f);
    // A perspective that doubles the depth at x = 4 halves the point.
    const float deep[16] = {1, 0, 0, 0.25f, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    x = 4.0f, y = 8.0f;
    Transformed(deep, x, y);
    CHECK_EQ(x, 2.0f);
    CHECK_EQ(y, 4.0f);
}

namespace
{
    // The corners of a box as the renderer keeps them, at the pixel centers Direct3D samples.
    std::vector<ScreenVertex> Corners(float left, float top, float right, float bottom)
    {
        std::vector<ScreenVertex> corners;
        for (const float x : {left, right})
            for (const float y : {top, bottom})
                corners.push_back(ScreenVertex{x - kPixelCenter, y - kPixelCenter, 0.0f, 1.0f, 0, 0.0f, 0.0f});
        return corners;
    }
}

TEST(a_clip_masks_box_goes_where_its_translation_and_transform_put_it)
{
    const std::vector<ScreenVertex> box = Corners(100.0f, 50.0f, 300.0f, 150.0f);
    CHECK((MaskEdges(box.data(), box.size(), 10.0f, 20.0f, nullptr) == PixelEdges{110, 70, 310, 170}));
    // A list sliding in from 6 pixels up clips 6 pixels up.
    const float slide[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, -6, 0, 1};
    CHECK((MaskEdges(box.data(), box.size(), 10.0f, 20.0f, slide) == PixelEdges{110, 64, 310, 164}));
    // Half the size about the origin, and an edge between two pixels goes to the nearer.
    const float half[16] = {0.5f, 0, 0, 0, 0, 0.5f, 0, 0, 0, 0, 1, 0, 0, 0.4f, 0, 1};
    CHECK((MaskEdges(box.data(), box.size(), 10.0f, 20.0f, half) == PixelEdges{55, 35, 155, 85}));
    // Twice the size: the corners are doubled, not the pixel centers the vertices are kept at.
    const float twice[16] = {2, 0, 0, 0, 0, 2, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    CHECK((MaskEdges(box.data(), box.size(), 10.0f, 20.0f, twice) == PixelEdges{220, 140, 620, 340}));
    // A quarter turn: the box around the turned corners, x from -170 to -70 and y from 110 to 310.
    const float turn[16] = {0, 1, 0, 0, -1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    CHECK((MaskEdges(box.data(), box.size(), 10.0f, 20.0f, turn) == PixelEdges{-170, 110, -70, 310}));
}

TEST(two_clips_leave_what_both_cover)
{
    CHECK((Shared(PixelEdges{0, 0, 100, 100}, PixelEdges{50, 20, 200, 80}) == PixelEdges{50, 20, 100, 80}));
    // Apart, they leave no pixel for the scissor to keep.
    const PixelEdges none = Shared(PixelEdges{0, 0, 10, 10}, PixelEdges{20, 20, 30, 30});
    CHECK(!ClampedRect(none.left, none.top, none.right, none.bottom, 80, 40));
}
