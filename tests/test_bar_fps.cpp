#include "bar_fps.h"
#include "test.h"

#include <cmath>

using namespace headsup;

namespace
{
    bool Near(double a, double b) { return std::fabs(a - b) < 1e-6; }
    constexpr double kThirtyFps = 1000.0 / 30.0; // ms: the game's usual frame divisor
}

TEST(nothing_is_reported_before_the_first_frame)
{
    CHECK(!FrameStats{}.Report().has_value());
    CHECK_EQ(FramesPerSecond(0.0), 0);
}

TEST(thirty_frames_a_second_read_as_30_fps)
{
    FrameStats stats;
    for (int i = 1; i <= 60; ++i)
        stats.Add(i / 30.0, kThirtyFps);
    const FrameReport r = *stats.Report();
    CHECK(Near(r.averageMs, kThirtyFps) && Near(r.slowestMs, kThirtyFps));
    CHECK_EQ(FramesPerSecond(r.averageMs), 30);
    CHECK_EQ(FramesPerSecond(1000.0 / 60.0), 60);
}

TEST(a_slow_frame_is_kept_for_five_seconds_and_the_average_covers_the_last_second)
{
    FrameStats stats;
    double t = 0.0;
    auto frame = [&](double ms) {
        t += ms / 1000.0;
        stats.Add(t, ms);
    };
    // Reports are taken at the first frame of each second.
    auto nextSecond = [&] {
        for (const double second = std::floor(t); std::floor(t) == second;)
            frame(kThirtyFps);
    };
    while (t < 10.0)
        frame(kThirtyFps);
    frame(120.0);
    const double hitch = std::floor(t);
    nextSecond();
    CHECK(stats.Report()->averageMs > kThirtyFps); // the hitch is in the last second
    while (std::floor(t) < hitch + 5.0)
        nextSecond();
    CHECK(Near(stats.Report()->slowestMs, 120.0));
    CHECK(Near(stats.Report()->averageMs, kThirtyFps));
    nextSecond();
    CHECK(Near(stats.Report()->slowestMs, kThirtyFps));
}

TEST(the_report_is_taken_at_each_whole_second_so_its_figures_change_once_a_second)
{
    FrameStats stats;
    double t = 0.0;
    auto frame = [&](double ms) {
        t += ms / 1000.0;
        stats.Add(t, ms);
    };
    while (t < 3.0)
        frame(kThirtyFps);
    const FrameReport steady = *stats.Report();
    CHECK(Near(steady.averageMs, kThirtyFps));
    // Frames that differ within the second change nothing shown until it ends.
    for (const double ms : {20.0, 45.0, 25.0, 50.0})
        frame(ms);
    CHECK(std::floor(t) == 3.0 && *stats.Report() == steady);
    while (std::floor(t) == 3.0)
        frame(kThirtyFps);
    CHECK(Near(stats.Report()->slowestMs, 50.0) && stats.Report()->averageMs > kThirtyFps);
}
