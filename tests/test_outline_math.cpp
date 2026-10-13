#include "outline_math.h"
#include "test.h"

#include <cmath>

using namespace headsup;

namespace
{
    void Ndc(const Mat4& p, float x, float y, float z, float& nx, float& ny)
    {
        const float cx = x * p.m[0] + y * p.m[4] + z * p.m[8] + p.m[12];
        const float cy = x * p.m[1] + y * p.m[5] + z * p.m[9] + p.m[13];
        const float cw = x * p.m[3] + y * p.m[7] + z * p.m[11] + p.m[15];
        nx = cx / cw;
        ny = cy / cw;
    }
}

TEST(identity_detection)
{
    Mat4 m{{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}};
    CHECK(IsIdentity(m));
    m.m[12] = 246.67f; // scenery: a real world translation
    CHECK(!IsIdentity(m));
}

TEST(shifted_projection_moves_points_by_the_same_ndc_offset_at_any_depth)
{
    // FFXI's right-handed projection (_34 = -1).
    const Mat4 p{{0.8961f, 0, 0, 0, 0, 1.5104f, 0, 0, 0, 0, -1, -1, 0, 0, -0.1f, 0}};
    const Mat4 s = ShiftProjection(p, 0.01f, -0.02f);
    for (float z : {-2.0f, -15.0f, -80.0f})
    {
        float ax, ay, bx, by;
        Ndc(p, 1.0f, 0.5f, z, ax, ay);
        Ndc(s, 1.0f, 0.5f, z, bx, by);
        CHECK(std::fabs((bx - ax) - 0.01f) < 1e-5f);
        CHECK(std::fabs((by - ay) + 0.02f) < 1e-5f);
    }
}

TEST(offsets_are_the_requested_pixels_from_the_centre_all_around)
{
    float sumX = 0.0f, sumY = 0.0f;
    for (int tap = 0; tap < 8; ++tap)
    {
        float dx, dy;
        OutlineOffset(tap, 8, 4.0f, 3840.0f, 2160.0f, dx, dy);
        const float px = dx * 3840.0f / 2.0f, py = dy * 2160.0f / 2.0f;
        CHECK(std::fabs(std::sqrt(px * px + py * py) - 4.0f) < 1e-3f);
        sumX += px, sumY += py;
    }
    CHECK(std::fabs(sumX) < 1e-3f && std::fabs(sumY) < 1e-3f); // evenly spaced, so they cancel out
    float dx, dy;
    OutlineOffset(0, 8, 4.0f, 3840.0f, 2160.0f, dx, dy);
    CHECK(std::fabs(dx * 1920.0f - 4.0f) < 1e-3f && std::fabs(dy) < 1e-6f); // the first copy goes right
    OutlineOffset(2, 8, 4.0f, 3840.0f, 2160.0f, dx, dy);
    CHECK(std::fabs(dx) < 1e-6f && std::fabs(dy * 1080.0f - 4.0f) < 1e-3f); // a quarter turn later, along y
}

TEST(an_outlines_thickness_is_a_percent_of_four_pixels)
{
    CHECK_EQ(OutlinePixels(100), 4.0f);
    CHECK_EQ(OutlinePixels(25), 1.0f); // the release's one pixel
    CHECK_EQ(OutlinePixels(400), 16.0f);
}

TEST(a_smoothness_level_is_four_more_copies_than_the_one_before)
{
    CHECK_EQ(SmoothnessCopies(SmoothnessLevel::Low), 4);
    CHECK_EQ(SmoothnessCopies(SmoothnessLevel::Medium), 8);
    CHECK_EQ(SmoothnessCopies(SmoothnessLevel::High), 12);
    CHECK_EQ(SmoothnessCopies(SmoothnessLevel::Ultra), 16);
    // A count from a file that could hold any number of copies reads as the nearest level, the higher of two as near.
    CHECK(SmoothnessLevelOf(4) == SmoothnessLevel::Low && SmoothnessLevelOf(5) == SmoothnessLevel::Low);
    CHECK(SmoothnessLevelOf(6) == SmoothnessLevel::Medium && SmoothnessLevelOf(9) == SmoothnessLevel::Medium);
    CHECK(SmoothnessLevelOf(11) == SmoothnessLevel::High && SmoothnessLevelOf(13) == SmoothnessLevel::High);
    CHECK(SmoothnessLevelOf(14) == SmoothnessLevel::Ultra && SmoothnessLevelOf(16) == SmoothnessLevel::Ultra);
    CHECK(SmoothnessLevelOf(0) == SmoothnessLevel::Low && SmoothnessLevelOf(99) == SmoothnessLevel::Ultra);
}

TEST(what_fades_in_is_whole_within_ten_yalms_and_a_hundredth_at_its_range)
{
    CHECK_EQ(FadeShare(0.0f, 50.0f), 1.0f);
    CHECK_EQ(FadeShare(10.0f, 50.0f), 1.0f);
    CHECK(std::fabs(FadeShare(50.0f, 50.0f) - 0.01f) < 1e-6f);
    CHECK(std::fabs(FadeShare(70.0f, 50.0f) - 0.01f) < 1e-6f);
    // Evenly between: half way from 10 to 50 it is half way from all to a hundredth.
    CHECK(std::fabs(FadeShare(30.0f, 50.0f) - 0.505f) < 1e-5f);
    CHECK(std::fabs(FadeShare(20.0f, 50.0f) - 0.7525f) < 1e-5f);
    CHECK(std::fabs(FadeShare(12.5f, 15.0f) - 0.505f) < 1e-5f);
    // A range at or inside the ten yalms leaves no stretch to fade over.
    CHECK_EQ(FadeShare(9.0f, 10.0f), 1.0f);
    CHECK(std::fabs(FadeShare(10.5f, 10.0f) - 0.01f) < 1e-6f);
}

TEST(an_outlines_alpha_is_its_opacity_and_with_fade_in_its_share_at_that_distance)
{
    CHECK_EQ(OutlineAlpha(100, 30.0f, 50.0f, false), 255);
    CHECK_EQ(OutlineAlpha(60, 30.0f, 50.0f, false), 153);
    CHECK_EQ(OutlineAlpha(60, 49.0f, 50.0f, false), 153); // no fading unless asked for
    CHECK_EQ(OutlineAlpha(60, 5.0f, 50.0f, true), 153);
    CHECK_EQ(OutlineAlpha(100, 30.0f, 50.0f, true), 129); // 255 * 0.505
    CHECK_EQ(OutlineAlpha(60, 30.0f, 50.0f, true), 77);   // 255 * 0.6 * 0.505
    CHECK_EQ(OutlineAlpha(60, 50.0f, 50.0f, true), 2);    // a hundredth of 153
}

TEST(an_alpha_tests_reference_shrinks_with_the_alpha_the_texture_is_multiplied_by)
{
    // The game cuts a mesh's texture where its alpha is not above 69. Under a color at alpha 153 of 255 a texel's
    // alpha comes out at 0.6 of its own, so the cut is at 0.6 of 69.
    CHECK_EQ(ScaledAlphaRef(69, 255), 69u);
    CHECK_EQ(ScaledAlphaRef(69, 153), 41u);
    CHECK_EQ(ScaledAlphaRef(69, 2), 0u);
    CHECK_EQ(ScaledAlphaRef(0, 153), 0u);
    CHECK_EQ(ScaledAlphaRef(255, 128), 128u);
}
