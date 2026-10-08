#include "pose.h"
#include "boxes.h"
#include "nameplate.h"
#include "test.h"

#include <cmath>

using namespace headsup;

namespace
{
    // The scene camera in a capture of a town (2560x1440 back buffer).
    const float kView[16]       = {-0.53312f, 0.0461647f, -0.844779f, 0.0f, -0.0f, -0.99851f, -0.0545656f, 0.0f, -0.84604f,
              -0.02909f, 0.532325f, 0.0f, -47.0549f, -6.67921f, 100.925f, 1.0f};
    const float kProjection[16] = {0.747742f, 0.0f, 0.0f, 0.0f, 0.0f, 1.26042f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, -1.0f, 0.0f, 0.0f,
        -0.1f, 0.0f};

    Camera TownCamera() { return MakeCamera(kView, kProjection, 1440.0f); }

    bool Near(float a, float b) { return test::Near(a, b, 0.5f); } // a pixel row

}

TEST(statuses_give_the_pose)
{
    CHECK(PoseFromStatus(0) == Pose::Standing);
    CHECK(PoseFromStatus(1) == Pose::Standing); // fighting
    CHECK(PoseFromStatus(33) == Pose::Resting);
    CHECK(PoseFromStatus(47) == Pose::Sitting);
    CHECK(PoseFromStatus(62) == Pose::Standing);
    CHECK(PoseFromStatus(63) == Pose::Chair); // the default chair
    CHECK(PoseFromStatus(83) == Pose::Chair); // the last chair key item
    CHECK(PoseFromStatus(84) == Pose::Standing);
}

TEST(sitting_for_aggro_is_phoenixs_rule)
{
    // Phoenix's isSitting: resting, sitting and the first eleven chairs let Too Weak aggressive mobs aggro; later chairs
    // only look seated.
    for (const uint32_t status : {33u, 47u, 63u, 73u})
        CHECK(IsSittingStatus(status));
    for (const uint32_t status : {0u, 1u, 62u, 74u, 83u})
        CHECK(!IsSittingStatus(status));
}

TEST(a_standing_players_name_stays_where_the_game_puts_it)
{
    const WorldPoint feet = FromEntityPosition(85.296f, -128.863f, -1.105f);
    CHECK(Near(PosedNameRow(TownCamera(), feet, 624.4f, Pose::Standing), 624.4f));
}

TEST(rows_and_heights_above_the_feet_convert_both_ways)
{
    // In the capture, Shio sat in a chair with the game's name bottom at row -51.4, above the screen, and Maryel sat on
    // the ground with it at row 662.7; their feet and those heights were found by bisection on the same camera.
    const Camera camera  = TownCamera();
    const WorldPoint shio = FromEntityPosition(61.957f, -97.996f, -0.917f);
    float row = 0.0f, height = 0.0f;
    CHECK(RowAtHeight(camera, shio, 0.0f, row) && Near(row, 733.53f));
    CHECK(HeightAtRow(camera, shio, -51.4f, height) && std::fabs(height - 2.9226f) < 0.01f);
    CHECK(HeightAtRow(camera, FromEntityPosition(83.920f, -99.868f, 0.297f), 662.7f, height) && std::fabs(height - 1.6548f) < 0.01f);
}

TEST(a_posed_players_name_comes_down_to_their_head)
{
    // The game put Shio's name 2.92 yalms above their feet in a chair, Maryel's 1.65 sitting and Justcool's 2.21
    // resting; each comes down to its pose's share of that height and stays above the feet.
    const Camera camera = TownCamera();
    struct Case
    {
        WorldPoint feet;
        float gameRow;
        Pose pose;
    };
    const Case cases[] = {{FromEntityPosition(61.957f, -97.996f, -0.917f), -51.4f, Pose::Chair},
        {FromEntityPosition(83.920f, -99.868f, 0.297f), 662.7f, Pose::Sitting},
        {FromEntityPosition(68.954f, -130.954f, -0.474f), 619.1f, Pose::Resting}};
    for (const Case& c : cases)
    {
        const float share = PoseHeightShare(c.pose);
        CHECK(share > 0.0f && share < 1.0f);
        float game = 0.0f, posed = 0.0f;
        CHECK(HeightAtRow(camera, c.feet, c.gameRow, game));
        CHECK(HeightAtRow(camera, c.feet, PosedNameRow(camera, c.feet, c.gameRow, c.pose), posed));
        CHECK(std::fabs(posed - share * game) < 0.01f);
    }
}

TEST(a_seated_players_name_is_placed_on_screen_when_the_games_is_above_it)
{
    // Shio in a chair, with the game's name above the top of the screen and their head in view.
    const Camera camera     = TownCamera();
    const WorldPoint feet   = FromEntityPosition(61.957f, -97.996f, -0.917f);
    const ScreenBox letters = test::Box(1945.6f, -102.1f, 2221.6f, -51.4f);
    const ScreenBox placed  = PlaceName(letters, &camera, feet, Pose::Chair);
    CHECK(!NameOnScreen(letters, 2560.0f, 1440.0f));
    CHECK(NameOnScreen(placed, 2560.0f, 1440.0f));
    CHECK(std::fabs(placed.Height() - letters.Height()) < 0.01f && std::fabs(placed.CenterX() - letters.CenterX()) < 0.01f);
    float game = 0.0f, ours = 0.0f;
    CHECK(HeightAtRow(camera, feet, letters.maxY, game) && HeightAtRow(camera, feet, placed.maxY, ours));
    CHECK(std::fabs(ours - PoseHeightShare(Pose::Chair) * game) < 0.01f);
}

TEST(a_name_that_cannot_be_projected_stays_put)
{
    // Behind the camera.
    const WorldPoint behind = FromEntityPosition(40.0f, -80.0f, 0.0f);
    CHECK(Near(PosedNameRow(TownCamera(), behind, 300.0f, Pose::Chair), 300.0f));
    float row = 0.0f;
    CHECK(!RowAtHeight(TownCamera(), behind, 0.0f, row));
}
