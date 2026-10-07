#include "outline_math.h"
#include "test.h"

#include <cmath>
#include <iterator>

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

namespace
{
    // The mob in these tests has no data, so its outline needs the unknown category on.
    Settings OutliningUnknown()
    {
        Settings s;
        s.show[static_cast<int>(Category::Unknown)] = true;
        return s;
    }
}

TEST(owner_is_the_first_tracked_pointer_of_any_kind)
{
    Tracker t;
    t.Update({ActorInput{0x1000, 1052, 0, EntityKind::Player, true, 0.0f, "Carrott", nullptr, std::nullopt, Pose::Standing, WorldPoint{}},
                 ActorInput{0x2000, 0, 0, EntityKind::Mob, true, 10.0f, "Beach Monk", nullptr, std::nullopt, Pose::Standing, WorldPoint{}}},
        PlayerState{20, false}, OutliningUnknown());

    const uint32_t playerDraw[] = {0x5, 0x1234, 0x1000, 0x2000}; // stale mob pointer above the live player
    const ActorInfo* owner      = FindOwner(std::begin(playerDraw), std::end(playerDraw), t);
    CHECK(owner != nullptr);
    CHECK(!owner->outline);

    const uint32_t mobDraw[] = {0x5, 0x2000, 0x1000};
    owner = FindOwner(std::begin(mobDraw), std::end(mobDraw), t);
    CHECK(owner != nullptr);
    CHECK(owner->outline);

    const uint32_t noActor[] = {0x5, 0x6};
    CHECK(FindOwner(std::begin(noActor), std::end(noActor), t) == nullptr);
}

TEST(stencil_formats)
{
    CHECK(HasStencilBits(75));
    CHECK(HasStencilBits(73));
    CHECK(HasStencilBits(79));
    CHECK(!HasStencilBits(77)); // D24X8
    CHECK(!HasStencilBits(80)); // D16
}
