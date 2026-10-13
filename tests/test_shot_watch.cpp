#include "shot_watch.h"
#include "test.h"

using namespace headsup;

// Ashita's screenshot plugin, asked to hide (/screenshot hide), has its fonts hidden from the start of the one frame
// it captures and puts them back at that frame's Present, before HeadsUp's own (probed on the test client,
// 2026-10-12: visible at Present of frame 69, hidden at every scene of frame 70, visible at its Present).
TEST(the_one_frame_ashitas_fonts_are_hidden_for_is_a_clean_screenshot)
{
    ShotWatch watch;
    CHECK(!watch.Seen(true) && !watch.Hiding());
    watch.EndFrame();
    CHECK(watch.Seen(false) && watch.Hiding());
    CHECK(watch.Seen(true)); // back before the frame ends: it is still that frame
    watch.EndFrame();
    CHECK(!watch.Seen(true) && !watch.Hiding()); // the frame after
}

TEST(fonts_hidden_part_way_through_a_frame_hide_the_rest_of_it)
{
    ShotWatch watch;
    CHECK(!watch.Seen(true));
    CHECK(watch.Seen(false));
    watch.EndFrame();
    CHECK(!watch.Hiding());
}

// Whatever hides them for longer is no screenshot: HeadsUp, and the game's windows with it, must not stay gone.
TEST(fonts_hidden_for_longer_than_a_screenshot_takes_bring_headsup_back)
{
    ShotWatch watch;
    for (int frame = 0; frame < kShotFrames; ++frame)
    {
        CHECK(watch.Seen(false));
        watch.EndFrame();
    }
    for (int frame = 0; frame < 5; ++frame)
    {
        CHECK(!watch.Seen(false));
        watch.EndFrame();
    }
    CHECK(!watch.Seen(true));
    watch.EndFrame();
    CHECK(watch.Seen(false)); // shown again in between, the next hide is a screenshot again
}
