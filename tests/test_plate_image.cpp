#include "plate_image.h"

#include "argb.h"
#include "test.h"

#include <algorithm>
#include <cmath>

using namespace headsup;

namespace
{
    Coverage Dot(int size, uint8_t alpha)
    {
        Coverage c{size, size, std::vector<uint8_t>(static_cast<size_t>(size * size), 0)};
        c.alpha[static_cast<size_t>(size / 2 * size + size / 2)] = alpha;
        return c;
    }

    uint32_t Pixel(const Image& image, int x, int y)
    {
        return image.argb[static_cast<size_t>(y * image.width + x)];
    }

    int At(const Coverage& c, int x, int y)
    {
        return c.alpha[static_cast<size_t>(y * c.width + x)];
    }

    int Alpha(uint32_t argb) { return static_cast<int>((argb >> 24) & 0xFFu); }
    bool Near(float a, float b) { return std::fabs(a - b) < 1e-4f; }
    int Green(uint32_t argb) { return static_cast<int>((argb >> 8) & 0xFFu); }

    // A filled square of side size inside an empty margin.
    Coverage Block(int size, int margin)
    {
        const int side = size + 2 * margin;
        Coverage c{side, side, std::vector<uint8_t>(static_cast<size_t>(side * side), 0)};
        for (int y = margin; y < margin + size; ++y)
            for (int x = margin; x < margin + size; ++x)
                c.alpha[static_cast<size_t>(y * side + x)] = 255;
        return c;
    }

    // How much of a long straight edge a Gaussian blur of this CSS radius (sigma half of it) carries to a pixel whose center
    // is past the edge by this far: the normal distribution's tail.
    float EdgeReach(float past, float blur)
    {
        return 0.5f * std::erfc(past / (blur / 2.0f * std::sqrt(2.0f)));
    }

    bool Near(int actual, float expected, float within) { return std::fabs(static_cast<float>(actual) - expected) <= within; }
}

TEST(text_sits_on_an_outline_grown_around_it)
{
    const Image image = Outlined(Dot(5, 255), 1, 0xFFFFFF80u, 0xFF000000u);
    CHECK_EQ(image.width, 5);
    CHECK_EQ(image.height, 5);
    CHECK_EQ(Pixel(image, 2, 2), 0xFFFFFF80u);
    for (const auto& [x, y] : {std::pair{1, 2}, {3, 2}, {2, 1}, {2, 3}, {1, 1}, {3, 3}})
        CHECK_EQ(Pixel(image, x, y), 0xFF000000u);
    for (const auto& [x, y] : {std::pair{0, 2}, {4, 2}, {2, 0}, {0, 0}})
        CHECK_EQ(Pixel(image, x, y), 0u);
}

TEST(partly_covered_text_blends_over_its_outline)
{
    // Coverage 128 is 0.502 for both the text and the outline under it: alpha 0.502 + 0.502 * 0.498 = 0.752, and the
    // color is 0.502 / 0.752 text over the black outline.
    const Image image = Outlined(Dot(3, 128), 1, 0xFFFFFFFFu, 0xFF000000u);
    CHECK_EQ(Pixel(image, 1, 1), 0xC0AAAAAAu);
    CHECK_EQ(Pixel(image, 0, 1), 0x80000000u);
}

TEST(sizes_round_to_whole_pixels_and_never_below_one)
{
    CHECK_EQ(WholePixels(44.49f), 44);
    CHECK_EQ(WholePixels(44.5f), 45);
    CHECK_EQ(WholePixels(0.4f), 1);
    CHECK_EQ(WholePixels(-3.0f), 1);
}

TEST(textures_round_up_to_a_power_of_two)
{
    for (const auto& [pixels, side] : {std::pair{1, 1}, {2, 2}, {5, 8}, {32, 32}, {33, 64}, {300, 512}})
        CHECK_EQ(TextureSide(pixels), side);
}

TEST(a_texts_texture_is_no_larger_than_the_device_makes_or_the_client_holds_many_of)
{
    CHECK_EQ(TextTextureSide(2048, 2048), 2048);
    CHECK_EQ(TextTextureSide(4096, 2048), 2048);   // the smaller of the device's two limits
    CHECK_EQ(TextTextureSide(2048, 4096), 2048);
    CHECK_EQ(TextTextureSide(1024, 1024), 1024);
    CHECK_EQ(TextTextureSide(3000, 3000), 2048);   // a 2049 px image is padded to 4096, so only powers of two count
    CHECK_EQ(TextTextureSide(16384, 16384), 4096); // 16384 by 1024 is 64 MB in a 32-bit client, for one text
    CHECK_EQ(TextTextureSide(0, 0), 2048);         // the device did not say
    CHECK_EQ(TextTextureSide(4096, 0), 2048);
}

TEST(the_arrow_points_down_inside_its_margin)
{
    const Coverage arrow = ShapeCoverage(ArrowShape(), 8, 2);
    CHECK_EQ(arrow.width, 14); // 10 wide: the arrow is 0.8 as tall as it is wide
    CHECK_EQ(arrow.height, 12);
    CHECK_EQ(At(arrow, 7, 2), 255);                      // the middle of its wide top edge
    CHECK(At(arrow, 2, 2) > 0 && At(arrow, 2, 2) < 255); // its top corners, on the slanted edges
    CHECK_EQ(At(arrow, 2, 2), At(arrow, 11, 2));         // and the same on both sides
    CHECK_EQ(At(arrow, 2, 8), 0);                        // beside its point
    CHECK_EQ(At(arrow, 11, 8), 0);
    CHECK(At(arrow, 7, 9) > 0); // its point
    for (int x = 0; x < arrow.width; ++x)
        CHECK_EQ(At(arrow, x, 0), 0); // the margin stays empty for the outline
}

TEST(each_shape_knows_where_it_points)
{
    CHECK(std::fabs(ShapeTip(ArrowShape()) - 0.5f) < 1e-4f); // the arrow points down from its middle
    const float feather = ShapeTip(FeatherShape());
    CHECK(feather > 0.17f && feather < 0.21f); // the feather's quill ends near its left side
}

TEST(a_square_fills_every_pixel)
{
    const ShapePoint square[] = {{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}};
    const Coverage c = ShapeCoverage(Shape{square, 4, 1.0f}, 5, 0);
    CHECK_EQ(c.width, 5);
    for (const uint8_t a : c.alpha)
        CHECK_EQ(a, 255);
}

TEST(the_feather_is_the_phoenix_icon)
{
    const Shape& feather = FeatherShape();
    CHECK(feather.count > 100);
    CHECK(feather.aspect > 2.1f && feather.aspect < 2.2f); // the icon's outline is 2.155 times as tall as wide
    for (size_t i = 0; i < feather.count; ++i)
        CHECK(feather.points[i].x >= 0.0f && feather.points[i].x <= 1.0f && feather.points[i].y >= 0.0f &&
              feather.points[i].y <= 1.0f);
    const Coverage c = ShapeCoverage(feather, 43, 0);
    CHECK_EQ(c.width, 20);
    int covered = 0;
    for (const uint8_t a : c.alpha)
        covered += a > 127 ? 1 : 0;
    CHECK(covered > c.width * c.height / 8 && covered < c.width * c.height / 2); // a slender feather, not a box
}

TEST(the_glow_is_the_letters_blurred_at_part_strength)
{
    // A 40 px block with a glow of CSS radius 8 (sigma 4) and no shadow, as the mockup's text-shadow 0 0 14px at 0.6.
    const Image image = Styled(Block(40, 16), Layers{0.0f, 0, 8.0f, 0xFFFFFFFFu, 0xFF000000u, 0xFF00FF00u});
    const int mid = 36;
    CHECK_EQ(Pixel(image, mid, mid), 0xFFFFFFFFu);                                 // the letters on top
    CHECK(Near(Alpha(Pixel(image, 15, mid)), 0.6f * 255.0f * EdgeReach(0.5f, 8.0f), 3.0f)); // one pixel out
    CHECK(Near(Alpha(Pixel(image, 9, mid)), 0.6f * 255.0f * EdgeReach(6.5f, 8.0f), 2.0f));  // seven out, nearly gone
    CHECK((Pixel(image, 15, mid) & 0x00FFFFFFu) == 0x0000FF00u);                   // in the glow's color
}

TEST(the_glow_is_the_same_either_side_of_the_letters)
{
    const Image image = Styled(Block(40, 16), Layers{0.0f, 0, 8.0f, 0xFFFFFFFFu, 0xFF000000u, 0xFF00FF00u});
    for (const int out : {1, 4, 9})
        CHECK_EQ(Pixel(image, 16 - out, 36), Pixel(image, 55 + out, 36)); // the block spans columns 16 to 55
    CHECK_EQ(Pixel(image, 36, 15), Pixel(image, 36, 56));
}

TEST(a_stronger_glow_is_denser_up_to_solid)
{
    Layers layers{0.0f, 0, 8.0f, 0xFFFFFFFFu, 0xFF000000u, 0xFF00FF00u};
    layers.glowStrength = 2.0f;
    CHECK(Near(Alpha(Pixel(Styled(Block(40, 16), layers), 15, 36)), 2.0f * 0.6f * 255.0f * EdgeReach(0.5f, 8.0f), 3.0f));
    layers.glowStrength = 5.0f; // 0.45 of the block reaches one pixel out, times 0.6 times 5 is past solid
    CHECK_EQ(Pixel(Styled(Block(40, 16), layers), 15, 36), 0xFF00FF00u);
}

TEST(the_dark_edge_is_a_soft_shadow_that_falls_a_little_below)
{
    // A shadow of CSS radius 4 (sigma 2), once in place and once 2 px lower, combined as two layers: a + b (1 - a).
    const Image image = Styled(Block(20, 8), Layers{4.0f, 2, 0.0f, 0xFFFFFFFFu, 0xFF000000u, 0});
    auto both = [](float a, float b) { return 255.0f * (a + b * (1.0f - a)); };
    const int mid = 18;
    // One pixel above the top: 0.5 px past the edge in place, 2.5 px past the dropped one.
    CHECK(Near(Alpha(Pixel(image, mid, 7)), both(EdgeReach(0.5f, 4.0f), EdgeReach(2.5f, 4.0f)), 4.0f));
    // One pixel below the bottom: 0.5 px past the edge in place, 1.5 px inside the dropped one.
    CHECK(Near(Alpha(Pixel(image, mid, 28)), both(EdgeReach(0.5f, 4.0f), 1.0f - EdgeReach(1.5f, 4.0f)), 4.0f));
    CHECK((Pixel(image, mid, 28) & 0x00FFFFFFu) == 0u); // black
    CHECK_EQ(Pixel(image, mid, mid), 0xFFFFFFFFu);
}

TEST(a_weaker_shadow_is_lighter_and_none_at_zero)
{
    Layers layers{4.0f, 2, 0.0f, 0xFFFFFFFFu, 0xFF000000u, 0};
    layers.shadowStrength = 0.5f;
    auto both = [](float a, float b) { return 255.0f * (a + b * (1.0f - a)); };
    CHECK(Near(Alpha(Pixel(Styled(Block(20, 8), layers), 18, 7)),
        both(0.5f * EdgeReach(0.5f, 4.0f), 0.5f * EdgeReach(2.5f, 4.0f)), 4.0f));
    layers.shadowStrength = 0.0f;
    CHECK_EQ(Pixel(Styled(Block(20, 8), layers), 18, 7), 0u);
}

TEST(no_glow_and_no_shadow_leave_just_the_letters)
{
    const Image image = Styled(Block(4, 4), Layers{0.0f, 0, 0.0f, 0xFFFFFFFFu, 0xFF000000u, 0xFF00FF00u});
    CHECK_EQ(Pixel(image, 3, 5), 0u);
    CHECK_EQ(Pixel(image, 4, 5), 0xFFFFFFFFu);
}

TEST(a_lighter_color_is_half_way_to_white)
{
    // The placeholder purple: B3 + (FF - B3) / 2 = D9, 4D + (FF - 4D) / 2 = A6, the mockup's [PH] color within two steps.
    CHECK_EQ(Lighter(0xFFB34DFFu), 0xFFD9A6FFu);
    CHECK_EQ(Lighter(0xFFFFFFFFu), 0xFFFFFFFFu);
    CHECK_EQ(Lighter(0xFF000000u), 0xFF7F7F7Fu);
}

TEST(the_ornament_is_a_hairline_that_brightens_toward_a_small_diamond)
{
    // 132 x 9 under a 22 px name, as in the mockup, with a diamond shadow of CSS radius 3 (a 3 px margin).
    const int width = 132, height = 9, margin = 3;
    const uint32_t red = 0xFFFF2626u;
    const Image image  = Ornament(width, height, 3.0f, 1.0f, red, 0xFF000000u);
    CHECK(image.width == width + 2 * margin && image.height == height + 2 * margin);
    const int line = margin + height / 2, center = margin + width / 2;
    CHECK(Alpha(Pixel(image, margin, line)) <= 4);                               // it fades in from nothing at each end
    CHECK(Alpha(Pixel(image, margin + width - 1, line)) <= 4);
    CHECK(Alpha(Pixel(image, margin + 39, line)) >= 250);                         // and is solid by 30% of the way
    CHECK((Pixel(image, margin + 39, line) & 0x00FFFFFFu) == (red & 0x00FFFFFFu)); // in the accent there
    const int nearer = Green(Pixel(image, margin + 52, line));                    // 40% of the way: toward white
    CHECK(nearer > Green(red) + 40 && nearer < 255);
    CHECK_EQ(Alpha(Pixel(image, margin + 39, line - 1)), 0);                      // one pixel thick
    CHECK_EQ(Pixel(image, center, line), Lighter(red));                           // the diamond, lighter
    CHECK(Alpha(Pixel(image, center, margin)) > 0);                               // as tall as the ornament
    const uint32_t above = Pixel(image, center, margin - 2);                      // with a dark shadow round it
    CHECK(Alpha(above) > 0 && (above & 0x00FFFFFFu) == 0u);
    CHECK_EQ(Pixel(Ornament(width, height, 3.0f, 0.0f, red, 0xFF000000u), center, margin - 2), 0u); // none at strength 0
    CHECK_EQ(Alpha(Pixel(image, margin + 20, line)), Alpha(Pixel(image, margin + width - 21, line))); // and symmetric
}

namespace
{
    uint8_t Alpha(const Image& image, int x, int y)
    {
        return Channel(image.argb[static_cast<size_t>(y * image.width + x)], kAlphaShift);
    }
}

TEST(the_strip_column_runs_its_gradient_then_its_lines_then_fades_below)
{
    const Image s = StripColumn(10, 4, 0xFF000000, 0xFF808080, 0xFFFFFFFF, 0xFFFF0000, 0xFF00FF00, 0x80FFFFFF);
    CHECK(s.width == 1 && s.height == 15);
    CHECK_EQ(Pixel(s, 0, 0), 0xFF000000u);
    CHECK(Channel(Pixel(s, 0, 3), kRedShift) > 0x00 && Channel(Pixel(s, 0, 3), kRedShift) < 0x80); // toward the middle
    CHECK_EQ(Pixel(s, 0, 9), 0xFFFF0000u);  // the bottom line
    CHECK_EQ(Pixel(s, 0, 10), 0xFF00FF00u); // the line under it
    CHECK(Alpha(s, 0, 11) > Alpha(s, 0, 14) && Alpha(s, 0, 11) <= 0x80 && Alpha(s, 0, 14) > 0);
    CHECK_EQ(StripColumn(10, 4, 0xFF000000, 0xFF000000, 0xFF000000, 0xFF000000, 0, 0).height, 10); // nothing under
}

TEST(the_strip_columns_middle_color_is_55_percent_down_its_gradient)
{
    // The gradient runs over rows 0 to 20 (the last row is the bottom line), so 55% down is row 11.
    const Image s = StripColumn(22, 0, 0xFF000000, 0xFFFFFFFF, 0xFF000000, 0xFF000000, 0, 0);
    CHECK_EQ(Pixel(s, 0, 11), 0xFFFFFFFFu);
    CHECK(Channel(Pixel(s, 0, 10), kRedShift) < 0xFF && Channel(Pixel(s, 0, 12), kRedShift) < 0xFF);
}

TEST(a_box_fills_from_its_top_color_to_its_bottom_color_by_each_rows_middle)
{
    const int height = 20;
    const Image b = Box(40, height, BoxStyle{.radius = 2.0f, .top = 0xFF000000, .bottom = 0xFFFFFFFF});
    for (int y = 0; y < height; ++y)
    {
        const auto gray = static_cast<uint8_t>(std::lround(255.0f * (static_cast<float>(y) + 0.5f) / height));
        CHECK_EQ(Pixel(b, 20, y), Opaque(gray, gray, gray));
    }
}

TEST(a_box_has_its_fill_inside_its_border_and_clear_corners)
{
    const Image b = Box(20, 10,
        BoxStyle{.radius = 3.0f, .top = 0xFF112233, .bottom = 0xFF112233, .border = 0xFFFFFFFF});
    CHECK(b.width == 20 && b.height == 10);
    CHECK_EQ(Pixel(b, 10, 5), 0xFF112233u);
    CHECK_EQ(Pixel(b, 0, 5), 0xFFFFFFFFu);
    CHECK_EQ(Alpha(b, 0, 0), 0);
    const Image framed = Box(20, 10,
        BoxStyle{.radius = 3.0f, .top = 0xFF112233, .bottom = 0xFF112233, .border = 0xFFFFFFFF,
            .outer = 0xFF00FF00, .outerGap = 2});
    CHECK(framed.width == 26 && framed.height == 16);
    CHECK_EQ(Pixel(framed, 0, 8), 0xFF00FF00u); // the outer border
    CHECK_EQ(Alpha(framed, 1, 8), 0);            // the gap
}

TEST(an_outer_border_takes_its_gap_and_its_own_pixel_past_the_box)
{
    const BoxStyle ringed{.radius = 3.0f, .border = 0xFFFFFFFF, .outer = 0xFF00FF00, .outerGap = 2};
    CHECK_EQ(OuterPad(ringed), 3);                                   // two clear pixels, then the ring's
    CHECK_EQ(OuterPad(BoxStyle{.radius = 3.0f, .outerGap = 2}), 0); // a gap with no ring around it
    const Image framed = Box(20, 10, ringed);
    CHECK(framed.width == 20 + 2 * OuterPad(ringed) && framed.height == 10 + 2 * OuterPad(ringed));
    CHECK_EQ(Pixel(framed, 0, 8), 0xFF00FF00u);
}

TEST(a_diamond_a_coin_and_a_fading_rule)
{
    const Image d = Diamond(9, 0xFFFFFFFF);
    CHECK(d.width == 9 && d.height == 9 && Alpha(d, 4, 4) == 0xFF && Alpha(d, 0, 0) == 0);
    const Image c = Coin(12, 0xFFE8C35A);
    CHECK(Alpha(c, 6, 6) == 0xFF && Alpha(c, 0, 0) == 0);
    CHECK(Channel(Pixel(c, 0, 6), kRedShift) < Channel(Pixel(c, 6, 6), kRedShift)); // the darker rim
    const Image r = FadingRule(10, 0xFFFFFFFF);
    CHECK(r.width == 10 && r.height == 1 && Alpha(r, 0, 0) > 0xF0 && Alpha(r, 9, 0) < 0x20);
}

namespace
{
    // Whether a point lies inside a w by h rect at the origin with corners of radius r: its core rect plus a disc of r.
    bool InsideRounded(float x, float y, float w, float h, float r)
    {
        const float cx = std::clamp(x, r, w - r), cy = std::clamp(y, r, h - r);
        return (x - cx) * (x - cx) + (y - cy) * (y - cy) <= r * r;
    }
}

TEST(a_box_no_wider_or_taller_than_its_border_is_just_border)
{
    const BoxStyle bordered{.radius = 3.0f, .top = 0xFF112233, .bottom = 0xFF112233, .border = 0xFFFFFFFF};
    const Image dot = Box(1, 1, bordered);
    CHECK(dot.width == 1 && dot.height == 1);
    CHECK_EQ(Pixel(dot, 0, 0), 0xFFFFFFFFu);
    const Image line = Box(60, 1, bordered);
    CHECK(line.width == 60 && line.height == 1);
    for (int x = 0; x < line.width; ++x)
        CHECK_EQ(Pixel(line, x, 0), 0xFFFFFFFFu); // every pixel is covered, and border colored
}

TEST(an_opaque_bordered_box_is_opaque_wherever_its_outline_covers_a_pixel_whole)
{
    const int width = 60, height = 28;
    const float radius = 14.0f;
    const Image box = Box(width, height,
        BoxStyle{.radius = radius, .top = 0xFF112233, .bottom = 0xFF112233, .border = 0xFFFFFFFF});
    auto inside = [&](float x, float y) { return InsideRounded(x, y, width, height, radius); };
    int covered = 0, lowest = 255;
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
        {
            const bool whole = inside(x, y) && inside(x + 1.0f, y) && inside(x, y + 1.0f) && inside(x + 1.0f, y + 1.0f);
            if (!whole) continue;
            ++covered;
            lowest = std::min(lowest, static_cast<int>(Alpha(box, x, y)));
        }
    CHECK(covered > 1000);
    CHECK_EQ(lowest, 255); // the fill and border shares of an arc pixel add up to all of it
}

TEST(an_opaque_coin_is_opaque_wherever_its_disc_covers_a_pixel_whole)
{
    const int size = 13;
    const float center = size / 2.0f;
    const Image coin = Coin(size, 0xFFE8C35A);
    auto inside = [&](float x, float y) {
        return (x - center) * (x - center) + (y - center) * (y - center) <= center * center;
    };
    int covered = 0, lowest = 255;
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x)
        {
            const bool whole = inside(x, y) && inside(x + 1.0f, y) && inside(x, y + 1.0f) && inside(x + 1.0f, y + 1.0f);
            if (!whole) continue;
            ++covered;
            lowest = std::min(lowest, static_cast<int>(Alpha(coin, x, y)));
        }
    CHECK(covered > 50);
    CHECK_EQ(lowest, 255); // the rim and the face add up to the whole disc
}

TEST(a_translucent_fill_keeps_its_share_with_or_without_a_border)
{
    const Image plain = Box(20, 10, BoxStyle{.radius = 3.0f, .top = 0xF7112233, .bottom = 0xF7112233});
    CHECK_EQ(Pixel(plain, 10, 5), 0xF7112233u);
    const Image bordered = Box(20, 10,
        BoxStyle{.radius = 3.0f, .top = 0xF7112233, .bottom = 0xF7112233, .border = 0xFFFFFFFF});
    CHECK_EQ(Alpha(bordered, 10, 5), 0xF7);
    CHECK_EQ(Pixel(bordered, 0, 5), 0xFFFFFFFFu); // the border stays opaque
}

TEST(a_sliced_box_keeps_the_boxs_sides_and_one_middle_column_for_the_rest)
{
    const BoxStyle styles[] = {
        BoxStyle{.radius = 4.95f, .top = 0xF71C160F, .bottom = 0xF70C0A08, .border = 0xFFC9A24A, .outer = 0xFF3D2F14,
            .outerGap = 2},
        BoxStyle{.radius = 7.65f, .top = 0xFF222226, .bottom = 0xFF222226, .border = 0xFF3A3A42},
        BoxStyle{.radius = 9.0f, .top = 0xFF2C2C33, .bottom = 0xFF2C2C33},
    };
    for (const BoxStyle& style : styles)
        for (const auto& [width, height] : {std::pair{394, 564}, {145, 36}, {40, 18}})
        {
            const Image box     = Box(width, height, style);
            const SlicedBox cut = SliceBox(width, height, style);
            CHECK(cut.width == box.width && cut.image.height == box.height);
            CHECK(cut.left > 0 && cut.right > 0 && cut.image.width == cut.left + kSliceMiddleColumns + cut.right);
            CHECK(cut.left + cut.right < box.width);
            for (int y = 0; y < box.height; ++y)
            {
                for (int x = 0; x < cut.left; ++x)
                    CHECK_EQ(Pixel(cut.image, x, y), Pixel(box, x, y));
                for (int x = cut.left; x < box.width - cut.right; ++x)
                    CHECK(Pixel(cut.image, cut.left, y) == Pixel(box, x, y) &&
                          Pixel(cut.image, cut.left + 1, y) == Pixel(box, x, y));
                for (int x = 0; x < cut.right; ++x)
                    CHECK_EQ(Pixel(cut.image, cut.left + kSliceMiddleColumns + x, y),
                        Pixel(box, box.width - cut.right + x, y));
            }
        }
}

TEST(a_box_too_narrow_for_a_middle_is_sliced_whole)
{
    const BoxStyle round{.radius = 9.0f, .top = 0xFF2C2C33, .bottom = 0xFF2C2C33, .border = 0xFFFFFFFF};
    const SlicedBox cut = SliceBox(18, 18, round);
    CHECK(cut.width == 18 && cut.left == 18 && cut.right == 0);
    CHECK(cut.image.argb == Box(18, 18, round).argb);
}

TEST(a_box_is_sliced_only_when_its_middle_has_three_columns_or_more)
{
    // Without a border, every column a 3 px radius leaves clear of the corners is alike: width - 6 of them.
    const BoxStyle plain{.radius = 3.0f, .top = 0xFF2C2C33, .bottom = 0xFF2C2C33};
    // The run of columns alike in every row around the image's center column.
    auto alike = [](const Image& box) {
        auto same = [&](int a, int b) {
            for (int y = 0; y < box.height; ++y)
                if (Pixel(box, a, y) != Pixel(box, b, y)) return false;
            return true;
        };
        const int center = box.width / 2;
        int first = center, last = center;
        while (first > 0 && same(first - 1, center))
            --first;
        while (last + 1 < box.width && same(last + 1, center))
            ++last;
        return last - first + 1;
    };
    const Image two = Box(8, 10, plain), three = Box(9, 10, plain);
    CHECK_EQ(alike(two), 2);
    CHECK_EQ(alike(three), 3);
    const SlicedBox whole = SliceBox(8, 10, plain);
    CHECK(whole.left == 8 && whole.right == 0 && whole.image.argb == two.argb);
    const SlicedBox sliced = SliceBox(9, 10, plain);
    CHECK(sliced.left == 3 && sliced.right == 3 && sliced.image.width == 3 + kSliceMiddleColumns + 3);
}

TEST(a_sliced_box_is_drawn_as_its_sides_with_its_first_middle_columns_center_stretched_between)
{
    const BoxStyle pill{.radius = 7.65f, .top = 0xFF222226, .bottom = 0xFF222226, .border = 0xFF3A3A42};
    constexpr float kU = 0.75f; // the image's share of its texture's width
    const SlicedBox cut = SliceBox(145, 36, pill);
    auto column = [&](float u) { return u / kU * static_cast<float>(cut.image.width); }; // an image column
    const std::vector<SliceSpan> spans = SliceSpans(cut, kU);
    CHECK_EQ(spans.size(), 3u);
    CHECK(spans[0].x == 0 && spans[0].width == cut.left && spans[1].x == cut.left);
    CHECK(spans[2].x == spans[1].x + spans[1].width && spans[2].x + spans[2].width == 145);
    CHECK_EQ(spans[2].width, cut.right);
    CHECK(spans[0].u0 == 0.0f && Near(column(spans[0].u1), static_cast<float>(cut.left)));
    // The middle takes one column's center, the first of the two, so filtering blends it with nothing else.
    CHECK(spans[1].u0 == spans[1].u1 && Near(column(spans[1].u0), static_cast<float>(cut.left) + 0.5f));
    CHECK(Near(column(spans[2].u0), static_cast<float>(cut.left + kSliceMiddleColumns)) && spans[2].u1 == kU);
    // A box with no middle is one span over the whole of its image.
    const SlicedBox whole = SliceBox(18, 18, pill);
    const std::vector<SliceSpan> one = SliceSpans(whole, kU);
    CHECK(one.size() == 1 && one[0].x == 0 && one[0].width == 18 && one[0].u0 == 0.0f && Near(one[0].u1, kU));
}

TEST(an_image_goes_in_a_textures_corner_and_the_rest_of_its_rows_is_cleared)
{
    const uint32_t image[] = {0xFF000001, 0xFF000002, 0xFF000003, 0xFF000004, 0xFF000005, 0xFF000006};
    constexpr int kSide = 4, kPitch = 5 * kBytesPerPixel; // a row holds one pixel more than the texture uses
    std::vector<uint32_t> rows(5 * kSide, 0xABABABABu);
    CopyToCorner(image, 3, 2, rows.data(), kPitch, kSide, kSide);
    for (int y = 0; y < kSide; ++y)
        for (int x = 0; x < 5; ++x)
        {
            const uint32_t expected = x == kSide ? 0xABABABABu : x < 3 && y < 2 ? image[y * 3 + x] : 0u;
            CHECK_EQ(rows[static_cast<size_t>(y * 5 + x)], expected);
        }
}

TEST(ink_is_the_box_of_the_pixels_covered_at_least_the_threshold)
{
    Coverage c{6, 5, std::vector<uint8_t>(30, 0)};
    c.alpha[1 * 6 + 2] = 200;
    c.alpha[3 * 6 + 4] = 64;
    c.alpha[4 * 6 + 0] = 63; // faint: not ink
    const InkBox ink = FindInk(c, 64);
    CHECK(!ink.Empty());
    CHECK(ink.left == 2 && ink.top == 1 && ink.right == 5 && ink.bottom == 4);
    CHECK(FindInk(Coverage{3, 3, std::vector<uint8_t>(9, 10)}, 64).Empty());
    CHECK(FindInk(Coverage{}, 64).Empty());
}

TEST(a_lines_ink_runs_from_its_glyphs_top_to_their_bottom_with_the_h_on_its_baseline)
{
    const InkBox glyphs{1, 2, 30, 17}, capital{0, 4, 9, 14}; // "H0gj" reaching above and below the H
    const std::optional<LineInk> ink = LineInkOf(glyphs, capital);
    CHECK(ink.has_value() && ink->top == 2.0f && ink->baseline == 14.0f && ink->bottom == 17.0f);
    CHECK(!LineInkOf(InkBox{}, capital).has_value()); // GDI drew nothing: no ink to keep
    CHECK(!LineInkOf(glyphs, InkBox{}).has_value());
}
