#include "plate_image.h"

#include "argb.h"
#include "test.h"

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

TEST(textures_round_up_to_a_power_of_two)
{
    for (const auto& [pixels, side] : {std::pair{1, 1}, {2, 2}, {5, 8}, {32, 32}, {33, 64}, {300, 512}})
        CHECK_EQ(TextureSide(pixels), side);
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

TEST(the_placeholder_mark_takes_its_own_color_from_its_column_on)
{
    Layers layers{0.0f, 0, 0.0f, 0xFFFFFFFFu, 0xFF000000u, 0};
    layers.markX     = 10;
    layers.markColor = 0xFFD9A6FFu;
    const Image image = Styled(Block(12, 4), layers);
    CHECK_EQ(Pixel(image, 9, 8), 0xFFFFFFFFu);
    CHECK_EQ(Pixel(image, 10, 8), 0xFFD9A6FFu);
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
