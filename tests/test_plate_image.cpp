#include "plate_image.h"
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
