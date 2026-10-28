#include "plate_image.h"

#include "argb.h"
#include "plate_settings.h"
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

TEST(a_picture_resized_is_the_average_of_what_each_new_pixel_covers)
{
    // Four by two, a red half and a clear half: at two by one, a red pixel and a clear one.
    Image picture{4, 2, std::vector<uint32_t>(8, 0)};
    for (int y = 0; y < 2; ++y)
        for (int x = 0; x < 2; ++x)
            picture.argb[static_cast<size_t>(y * 4 + x)] = 0xFFFF0000u;
    const Image half = Resized(picture, 2, 1);
    CHECK(half.width == 2 && half.height == 1);
    CHECK_EQ(Pixel(half, 0, 0), 0xFFFF0000u);
    CHECK_EQ(Pixel(half, 1, 0), 0u);
    // One pixel of it all: half covered, and red, not red darkened by the clear half's black.
    CHECK_EQ(Pixel(Resized(picture, 1, 1), 0, 0), 0x80FF0000u);
    // Enlarged, each pixel becomes a block of its own color.
    const Image twice = Resized(picture, 8, 4);
    CHECK(twice.width == 8 && twice.height == 4);
    CHECK_EQ(Pixel(twice, 3, 3), 0xFFFF0000u);
    CHECK_EQ(Pixel(twice, 4, 0), 0u);
    // Its own size gives it back.
    CHECK(Resized(picture, 4, 2).argb == picture.argb);
}

TEST(a_picture_keeps_its_colors_inside_an_edge_grown_around_it)
{
    // One blue pixel beside one green: with an edge of 1, they stand in the middle of an image two pixels larger
    // each way, and every pixel touching them is the edge's color.
    Image picture{2, 1, {0xFF0000FFu, 0xFF00FF00u}};
    const Image edged = Edged(picture, 1, 0xFF101010u);
    CHECK(edged.width == 4 && edged.height == 3);
    CHECK_EQ(Pixel(edged, 1, 1), 0xFF0000FFu);
    CHECK_EQ(Pixel(edged, 2, 1), 0xFF00FF00u);
    for (const auto& [x, y] : {std::pair{0, 1}, {3, 1}, {1, 0}, {2, 2}, {0, 0}, {3, 2}})
        CHECK_EQ(Pixel(edged, x, y), 0xFF101010u);
    // A clear pixel of the picture next to a covered one is edge too; far from any, nothing.
    Image apart{5, 1, {0xFFFFFFFFu, 0u, 0u, 0u, 0u}};
    const Image far = Edged(apart, 1, 0xFF101010u);
    CHECK_EQ(Pixel(far, 2, 1), 0xFF101010u);
    CHECK_EQ(Pixel(far, 4, 1), 0u);
    // A half clear pixel shows the edge through it.
    Image faint{1, 1, {0x80FFFFFFu}};
    CHECK_EQ(Pixel(Edged(faint, 1, 0xFF000000u), 1, 1), 0xC0AAAAAAu);
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
    CHECK_EQ(ShapeWidth(ArrowShape(), 8), 10); // the arrow is 0.8 as tall as it is wide
    const Coverage arrow = ShapeCoverage(ArrowShape(), 10, 8, 2);
    CHECK_EQ(arrow.width, 14);
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
    const Coverage c = ShapeCoverage(Shape{square, 4, 1.0f}, 7, 5, 0);
    CHECK(c.width == 7 && c.height == 5); // stretched to the width and height asked for, whatever its own
    for (const uint8_t a : c.alpha)
        CHECK_EQ(a, 255);
}

TEST(a_shape_is_stretched_to_the_width_and_height_asked_for)
{
    // The arrow twice as wide as it would be at this height: still its wide top edge, its slanted sides and its
    // point under the middle.
    const Coverage wide = ShapeCoverage(ArrowShape(), 20, 8, 0);
    CHECK(wide.width == 20 && wide.height == 8);
    CHECK_EQ(At(wide, 10, 0), 255);
    CHECK_EQ(At(wide, 1, 7), 0);
    CHECK_EQ(At(wide, 18, 7), 0);
    CHECK(At(wide, 9, 7) > 0 && At(wide, 10, 7) > 0);
    for (int y = 0; y < wide.height; ++y)
        for (int x = 0; x < wide.width / 2; ++x)
            CHECK_EQ(At(wide, x, y), At(wide, wide.width - 1 - x, y));
    // Narrow and tall, each row is as wide as the same row of the wide one, by their widths.
    const Coverage narrow = ShapeCoverage(ArrowShape(), 5, 8, 0);
    const auto rowSum = [](const Coverage& c, int y) {
        int sum = 0;
        for (int x = 0; x < c.width; ++x)
            sum += At(c, x, y);
        return sum;
    };
    for (int y = 0; y < 8; ++y)
        CHECK(std::abs(rowSum(wide, y) - 4 * rowSum(narrow, y)) <= 8);
}

TEST(the_feather_is_the_phoenix_icon)
{
    const Shape& feather = FeatherShape();
    CHECK(feather.count > 100);
    CHECK(feather.aspect > 2.1f && feather.aspect < 2.2f); // the icon's outline is 2.155 times as tall as wide
    for (size_t i = 0; i < feather.count; ++i)
        CHECK(feather.points[i].x >= 0.0f && feather.points[i].x <= 1.0f && feather.points[i].y >= 0.0f &&
              feather.points[i].y <= 1.0f);
    CHECK_EQ(ShapeWidth(feather, 43), 20);
    const Coverage c = ShapeCoverage(feather, 20, 43, 0);
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
    const Image s = StripColumn(10, 4, 0xFF000000, 0xFF808080, 0xFFFFFFFF, 0xFFFF0000, 0xFF00FF00, 0x80FFFFFF, 1);
    CHECK(s.width == 1 && s.height == 15);
    CHECK_EQ(Pixel(s, 0, 0), 0xFF000000u);
    CHECK(Channel(Pixel(s, 0, 3), kRedShift) > 0x00 && Channel(Pixel(s, 0, 3), kRedShift) < 0x80); // toward the middle
    CHECK_EQ(Pixel(s, 0, 9), 0xFFFF0000u);  // the bottom line
    CHECK_EQ(Pixel(s, 0, 10), 0xFF00FF00u); // the line under it
    CHECK(Alpha(s, 0, 11) > Alpha(s, 0, 14) && Alpha(s, 0, 11) <= 0x80 && Alpha(s, 0, 14) > 0);
    CHECK_EQ(StripColumn(10, 4, 0xFF000000, 0xFF000000, 0xFF000000, 0xFF000000, 0, 0, 1).height, 10); // nothing under
}

TEST(the_strip_columns_border_is_its_last_rows_in_the_line_color)
{
    const Image thick = StripColumn(10, 0, 0xFF000000, 0xFF808080, 0xFFFFFFFF, 0xFFFF0000, 0, 0, 3);
    CHECK(Pixel(thick, 0, 7) == 0xFFFF0000u && Pixel(thick, 0, 8) == 0xFFFF0000u && Pixel(thick, 0, 9) == 0xFFFF0000u);
    CHECK_EQ(Pixel(thick, 0, 6), 0xFFFFFFFFu); // the gradient ends on its bottom color just over the border
    const Image none = StripColumn(10, 0, 0xFF000000, 0xFF808080, 0xFFFFFFFF, 0xFFFF0000, 0, 0, 0);
    CHECK_EQ(Pixel(none, 0, 9), 0xFFFFFFFFu);
}

TEST(the_strip_columns_middle_color_is_55_percent_down_its_gradient)
{
    // The gradient runs over rows 0 to 20 (the last row is the bottom line), so 55% down is row 11.
    const Image s = StripColumn(22, 0, 0xFF000000, 0xFFFFFFFF, 0xFF000000, 0xFF000000, 0, 0, 1);
    CHECK_EQ(Pixel(s, 0, 11), 0xFFFFFFFFu);
    CHECK(Channel(Pixel(s, 0, 10), kRedShift) < 0xFF && Channel(Pixel(s, 0, 12), kRedShift) < 0xFF);
}

TEST(an_outer_border_reaches_its_gap_and_its_own_pixel_past_the_box)
{
    CHECK_EQ(OuterPad(BoxStyle{.radius = 3.0f, .border = 0xFFFFFFFF, .outer = 0xFF00FF00, .outerGap = 2}), 3);
    CHECK_EQ(OuterPad(BoxStyle{.radius = 3.0f}), 0);
    CHECK_EQ(OuterPad(BoxStyle{.radius = 3.0f, .outerGap = 2}), 0); // a gap with no ring around it
}

TEST(a_diamond_and_a_coin)
{
    const Image d = Diamond(9, 0xFFFFFFFF);
    CHECK(d.width == 9 && d.height == 9 && Alpha(d, 4, 4) == 0xFF && Alpha(d, 0, 0) == 0);
    const Image c = Coin(12, 0xFFE8C35A);
    CHECK(Alpha(c, 6, 6) == 0xFF && Alpha(c, 0, 0) == 0);
    CHECK(Channel(Pixel(c, 0, 6), kRedShift) < Channel(Pixel(c, 6, 6), kRedShift)); // the darker rim
}

namespace
{
    // Whether a point is in a five-pointed star with a point straight up, in the circle of radius half around
    // (half, half): the even-odd rule over its ten corners, the notches at a pentagram's (3 - sqrt 5) / 2 of the
    // points' radius.
    bool InsideStar(float x, float y, float half)
    {
        const float pi = 3.14159265f, notch = (3.0f - std::sqrt(5.0f)) / 2.0f;
        const auto corner = [&](int k) {
            const float radius = half * (k % 2 == 0 ? 1.0f : notch), angle = static_cast<float>(k % 10) * pi / 5.0f;
            return std::pair<float, float>{half + radius * std::sin(angle), half - radius * std::cos(angle)};
        };
        bool inside = false;
        for (int k = 0; k < 10; ++k)
        {
            const auto [x0, y0] = corner(k);
            const auto [x1, y1] = corner(k + 1);
            if ((y0 > y) != (y1 > y) && x < x0 + (x1 - x0) * (y - y0) / (y1 - y0)) inside = !inside;
        }
        return inside;
    }
}

TEST(a_star_covers_each_pixel_as_far_as_a_five_pointed_star_with_a_point_up_does)
{
    constexpr int kGrid = 16; // points across a pixel
    for (const int size : {11, 16, 23})
    {
        const Image star = Star(size, 0xFFE8C35A);
        CHECK(star.width == size && star.height == size);
        const float half = static_cast<float>(size) / 2.0f;
        float worst      = 0.0f;
        for (int y = 0; y < size; ++y)
            for (int x = 0; x < size; ++x)
            {
                int in = 0;
                for (int sy = 0; sy < kGrid; ++sy)
                    for (int sx = 0; sx < kGrid; ++sx)
                        in += InsideStar(static_cast<float>(x) + (static_cast<float>(sx) + 0.5f) / kGrid,
                            static_cast<float>(y) + (static_cast<float>(sy) + 0.5f) / kGrid, half);
                worst = std::max(worst, std::fabs(static_cast<float>(Alpha(star, x, y)) / 255.0f -
                                                  static_cast<float>(in) / (kGrid * kGrid)));
            }
        CHECK(worst < 0.15f);
        CHECK(Pixel(star, size / 2, size / 2) == 0xFFE8C35Au && Alpha(star, 0, 0) == 0);
        CHECK(Alpha(star, size / 2, 0) > 0 && Alpha(star, size / 2, size - 1) == 0); // a point up, a notch down
    }
}

TEST(a_fold_arrow_points_right_while_closed_and_down_while_open)
{
    for (const int size : {7, 10, 13})
    {
        const Image closed = FoldArrow(size, 0xFFB0A8C0, false), open = FoldArrow(size, 0xFFB0A8C0, true);
        CHECK(closed.width == size && closed.height == size && open.width == size && open.height == size);
        const int mid = size / 2;
        // Closed: solid along its left edge's middle, clear at the right corners, its tip at the right edge's middle.
        CHECK(Alpha(closed, 0, mid) > 200 && Alpha(closed, size - 1, 0) == 0 && Alpha(closed, size - 1, size - 1) == 0);
        CHECK(Alpha(closed, size - 1, mid) > 0);
        // Open is the same triangle turned to point down.
        for (int y = 0; y < size; ++y)
            for (int x = 0; x < size; ++x)
                CHECK_EQ(Alpha(open, x, y), Alpha(closed, y, x));
    }
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

namespace
{
    // Whether a point is in a circular arrow, from its square's middle with y up: a ring open for the eighth
    // counterclockwise of straight up, and at the ring's top a head pointing left into that gap.
    bool InsideResetArrow(float x, float y, float size)
    {
        const float pi = 3.14159265f, radius = 0.32f * size, length = 0.26f * size;
        float turn = std::atan2(-x, y); // counterclockwise from straight up
        if (turn < 0.0f) turn += 2.0f * pi;
        const bool ring = std::fabs(std::hypot(x, y) - radius) <= 0.055f * size && turn >= pi / 4.0f;
        const bool head = x <= 0.0f && x >= -length && std::fabs(y - radius) <= 0.17f * size * (x + length) / length;
        return ring || head;
    }
}

TEST(the_reset_arrow_turns_counterclockwise_to_a_head_at_its_top)
{
    constexpr int kGrid = 16; // points across a pixel
    for (const int size : {16, 32, 45})
    {
        const Image arrow = ResetArrow(size, 0xFFFFFFFFu);
        CHECK(arrow.width == size && arrow.height == size);
        const float half = static_cast<float>(size) / 2.0f;
        float worst      = 0.0f;
        for (int y = 0; y < size; ++y)
            for (int x = 0; x < size; ++x)
            {
                int in = 0;
                for (int sy = 0; sy < kGrid; ++sy)
                    for (int sx = 0; sx < kGrid; ++sx)
                        in += InsideResetArrow(static_cast<float>(x) + (static_cast<float>(sx) + 0.5f) / kGrid - half,
                            half - static_cast<float>(y) - (static_cast<float>(sy) + 0.5f) / kGrid, static_cast<float>(size));
                worst = std::max(worst, std::fabs(static_cast<float>(Alpha(arrow, x, y)) / 255.0f -
                                                  static_cast<float>(in) / (kGrid * kGrid)));
            }
        CHECK(worst < 0.15f);
        // Its ring passes the right, the bottom and the left, and its middle and corners are clear.
        const int reach = static_cast<int>(0.32f * static_cast<float>(size));
        CHECK(Alpha(arrow, size / 2 + reach, size / 2) > 128);
        CHECK(Alpha(arrow, size / 2, size / 2 + reach) > 128);
        CHECK(Alpha(arrow, size / 2 - reach - 1, size / 2) > 128);
        CHECK(Alpha(arrow, size / 2, size / 2) == 0 && Alpha(arrow, 0, 0) == 0 && Alpha(arrow, size - 1, size - 1) == 0);
    }
}

namespace
{
    int RowInk(const Coverage& c, int y)
    {
        int ink = 0;
        for (int x = 0; x < c.width; ++x)
            ink += At(c, x, y) > 127 ? 1 : 0;
        return ink;
    }
}

TEST(a_mark_is_a_small_shape_in_a_line_as_tall_as_its_text)
{
    // For text 40 pixels tall inside a margin of 3: as tall as the text's own image, and a little over its middle.
    for (const DashMark mark : {DashMark::Dot, DashMark::Square, DashMark::Diamond})
    {
        const Coverage c = MarkCoverage(mark, 40, 3);
        CHECK_EQ(c.height, 46);
        CHECK(c.width > 6 + 8 && c.width < 6 + 30); // narrower than it is tall, with room either side
        int first = c.height, last = -1, widest = 0;
        for (int y = 0; y < c.height; ++y)
        {
            if (RowInk(c, y) == 0) continue;
            first  = std::min(first, y);
            last   = y;
            widest = std::max(widest, RowInk(c, y));
        }
        CHECK(first > 3 + 8 && last < 3 + 36);            // clear of the letters' tops and of their feet
        CHECK(last - first + 1 >= 8 && last - first + 1 <= 18);
        const int middle = (first + last) / 2;
        CHECK(middle >= 3 + 20 && middle <= 3 + 27);      // where a dash sits, a little under the line's middle
        for (int y = 0; y < c.height; ++y)                 // the same either side of its middle
            for (int x = 0; x < c.width / 2; ++x)
                CHECK(std::abs(At(c, x, y) - At(c, c.width - 1 - x, y)) <= 2);
        // A square is as wide in every row; a diamond and a dot are widest in their middle and narrow to their ends.
        if (mark == DashMark::Square)
            CHECK(RowInk(c, first + 1) == widest && RowInk(c, last - 1) == widest);
        else
            CHECK(RowInk(c, middle) == widest && RowInk(c, first) < widest && RowInk(c, last) < widest);
        // A diamond comes to a point; a dot is still wide a quarter of its way down.
        const int quarter = first + (last - first) / 4;
        if (mark == DashMark::Diamond) CHECK(RowInk(c, quarter) * 10 <= widest * 7);
        if (mark == DashMark::Dot) CHECK(RowInk(c, quarter) * 10 >= widest * 8);
    }
}

TEST(pieces_are_joined_side_by_side_into_one_line_with_one_margin_around_it)
{
    // Two pieces with a margin of 1 around a 2 by 2 and a 3 by 2 block.
    Coverage a{4, 4, std::vector<uint8_t>(16, 0)}, b{5, 4, std::vector<uint8_t>(20, 0)};
    for (int y = 1; y < 3; ++y)
    {
        for (int x = 1; x < 3; ++x)
            a.alpha[static_cast<size_t>(y * 4 + x)] = 200;
        for (int x = 1; x < 4; ++x)
            b.alpha[static_cast<size_t>(y * 5 + x)] = 100;
    }
    const Coverage joined = Joined({a, b}, {}, 1);
    CHECK(joined.width == 2 + 3 + 2 && joined.height == 4);
    for (int x = 0; x < joined.width; ++x)
    {
        CHECK_EQ(At(joined, x, 0), 0);
        CHECK_EQ(At(joined, x, 1), x == 0 || x == 6 ? 0 : x < 3 ? 200 : 100);
    }
    // One piece is itself, and none is an empty line.
    CHECK(Joined({a}, {}, 1).alpha == a.alpha);
    CHECK(Joined({}, {}, 1).width == 2 && Joined({}, {}, 1).alpha.empty() == false);
}

TEST(a_piece_stands_in_the_room_the_one_before_it_leaves_for_its_last_letters_lean)
{
    // Two pieces of italic letters, 6 wide with 2 of room for the last letter's lean, a mark 3 wide between them and
    // a margin of 1: the mark starts where the first piece's cells end, and only the last piece keeps its room.
    const std::vector<int> widths{8, 3, 8}, room{2, 0, 2};
    CHECK_EQ(JoinedWidth(widths, room), 6 + 3 + 8);
    CHECK_EQ(JoinedWidth(widths, {}), 8 + 3 + 8); // upright: nothing to stand in
    CHECK_EQ(JoinedWidth({}, {}), 0);
    const auto piece = [](int width, uint8_t ink) {
        Coverage c{width + 2, 3, std::vector<uint8_t>(static_cast<size_t>((width + 2) * 3), 0)};
        for (int x = 1; x <= width; ++x)
            c.alpha[static_cast<size_t>(c.width + x)] = ink;
        return c;
    };
    Coverage letters = piece(8, 100);
    letters.alpha[static_cast<size_t>(letters.width + 7)] = 30; // the lean, faint in its room
    letters.alpha[static_cast<size_t>(letters.width + 8)] = 0;
    const Coverage joined = Joined({letters, piece(3, 255), letters}, room, 1);
    CHECK_EQ(joined.width, 17 + 2);
    const std::vector<int> row{0, 100, 100, 100, 100, 100, 100, 255, 255, 255, 100, 100, 100, 100, 100, 100, 30, 0, 0};
    for (int x = 0; x < joined.width; ++x)
        CHECK_EQ(At(joined, x, 1), row[static_cast<size_t>(x)]);
    // What leans into the room stays where the next piece is fainter than it.
    const Coverage faint = Joined({letters, piece(3, 10)}, room, 1);
    CHECK_EQ(At(faint, 7, 1), 30);
    CHECK_EQ(At(faint, 8, 1), 10);
    // Only italics lean.
    CHECK(ItalicLean(40, StyleBit(FontStyle::Italic)) > 0);
    CHECK_EQ(ItalicLean(40, StyleBit(FontStyle::Bold) | StyleBit(FontStyle::Underline)), 0);
}

TEST(a_line_is_drawn_under_the_whole_of_a_text_inside_its_margin)
{
    Coverage text{12, 9, std::vector<uint8_t>(12 * 9, 0)};
    text.alpha[static_cast<size_t>(4 * 12 + 5)] = 77; // a letter
    Underline(text, 6, 2, 2);
    for (int y = 0; y < text.height; ++y)
        for (int x = 0; x < text.width; ++x)
        {
            const bool under = (y == 6 || y == 7) && x >= 2 && x < 10;
            CHECK_EQ(At(text, x, y), under ? 255 : y == 4 && x == 5 ? 77 : 0);
        }
    // Past the image's last row it is cut, and one of no thickness is none.
    Coverage low{6, 4, std::vector<uint8_t>(24, 0)};
    Underline(low, 3, 5, 1);
    CHECK(At(low, 2, 3) == 255 && At(low, 2, 2) == 0);
    Coverage none{6, 4, std::vector<uint8_t>(24, 0)};
    Underline(none, 1, 0, 1);
    Underline(none, 9, 2, 1);
    CHECK(none.alpha == std::vector<uint8_t>(24, 0));
}

TEST(an_underline_is_two_pixels_thick_at_least_and_a_twelfth_of_a_tall_text)
{
    // A font's one row would come and go as its text is drawn a little smaller or larger than its image.
    CHECK_EQ(UnderlineThickness(8, 1), 2);
    CHECK_EQ(UnderlineThickness(15, 1), 2);
    CHECK_EQ(UnderlineThickness(24, 1), 2);
    CHECK_EQ(UnderlineThickness(48, 1), 4);
    CHECK_EQ(UnderlineThickness(30, 1), 3); // 2.5, to the nearer whole pixel away from zero
    CHECK_EQ(UnderlineThickness(24, 3), 3); // a font's own thicker band is kept
}
