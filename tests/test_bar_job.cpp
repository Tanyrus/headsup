#include "bar_job.h"
#include "test.h"

#include <cmath>

using namespace headsup;

namespace
{
    constexpr uint8_t kWar = 1, kWhm = 3, kNin = 13;
    constexpr uint32_t kMe = 0x1234, kAnother = 0x5678; // server ids

    JobSample Sample(uint8_t level, uint32_t exp, uint32_t needed)
    {
        JobSample s;
        s.mainJob   = kWar;
        s.mainLevel = level;
        s.subJob    = kNin;
        s.subLevel  = static_cast<uint8_t>(level / 2);
        s.exp       = exp;
        s.expNeeded = needed;
        return s;
    }

    // The plugin updates every frame while the widget shows; a second apart is as sparse as the tests need.
    void Run(ExpTracker& t, double from, double to, const JobSample& s)
    {
        for (double at = from; at <= to; at += 1.0)
            t.Update(at, s, kMe);
    }

    JobSample AtCap(uint32_t merits, uint32_t limitPoints)
    {
        JobSample s   = Sample(kLevelCap, 0, 0);
        s.merits      = merits;
        s.meritsMax   = 10;
        s.limitPoints = limitPoints;
        s.limitMode   = true;
        return s;
    }
}

TEST(jobs_read_as_the_game_abbreviates_them)
{
    CHECK_EQ(JobsText(kWar, 75, kNin, 37), std::string("WAR 75 / NIN 37"));
    CHECK_EQ(JobsText(16, 70, 0, 0), std::string("BLU 70")); // no subjob
    CHECK_EQ(JobsText(18, 60, 7, 0), std::string("PUP 60")); // a subjob not yet leveled
    CHECK_EQ(JobsText(0, 0, 0, 0), std::string("-"));
    CHECK_EQ(JobName(22), "RUN");
    CHECK_EQ(JobName(23), "");
}

TEST(exp_gained_counts_within_a_level_and_across_a_level_up)
{
    CHECK_EQ(Gained(Sample(30, 1000, 7600), Sample(30, 1300, 7600)), 300u);
    CHECK_EQ(Gained(Sample(30, 7400, 7600), Sample(31, 150, 8000)), 350u); // 200 to finish 30, then 150 into 31
}

TEST(nothing_counts_for_a_death_a_job_change_or_a_level_lost_or_synced)
{
    CHECK_EQ(Gained(Sample(30, 1300, 7600), Sample(30, 1000, 7600)), 0u);
    JobSample whm = Sample(30, 1300, 7600);
    whm.mainJob   = kWhm;
    CHECK_EQ(Gained(Sample(30, 1000, 7600), whm), 0u);
    CHECK_EQ(Gained(Sample(31, 100, 8000), Sample(30, 7500, 7600)), 0u);
    CHECK_EQ(Gained(Sample(30, 1000, 7600), Sample(32, 100, 8400)), 0u); // a sync lifted, not EXP
}

TEST(at_the_cap_limit_points_count_and_a_merit_is_ten_thousand_of_them)
{
    CHECK_EQ(Gained(AtCap(3, 9800), AtCap(4, 200)), 400u);
    CHECK_EQ(Gained(AtCap(4, 200), AtCap(4, 650)), 450u);
    CHECK_EQ(Gained(AtCap(4, 650), AtCap(3, 650)), 0u);     // a merit spent
    CHECK_EQ(Gained(AtCap(10, 9999), AtCap(10, 9999)), 0u); // at the merit cap
}

TEST(exp_per_hour_is_taken_over_the_last_ten_minutes_after_a_minute_of_them)
{
    ExpTracker t;
    Run(t, 0.0, 29.0, Sample(30, 1000, 7600));
    Run(t, 30.0, 89.0, Sample(30, 1300, 7600));
    CHECK(!t.PerHour(50.0).has_value()); // under a minute tracked
    Run(t, 90.0, 120.0, Sample(30, 1600, 7600));
    CHECK_EQ(*t.PerHour(120.0), 600.0 * 3600.0 / 120.0);
    CHECK_EQ(*t.PerHour(650.0), 300.0 * 3600.0 / 600.0); // the gain at 30 s has left the window
    CHECK(!t.PerHour(700.0).has_value());               // nothing in the last ten minutes
}

TEST(a_job_change_starts_the_rate_again)
{
    ExpTracker t;
    Run(t, 0.0, 99.0, Sample(30, 1000, 7600));
    Run(t, 100.0, 199.0, Sample(30, 2000, 7600));
    JobSample whm = Sample(20, 500, 3000);
    whm.mainJob   = kWhm;
    Run(t, 200.0, 249.0, whm);
    CHECK(!t.PerHour(230.0).has_value());
    whm.exp = 800;
    Run(t, 250.0, 300.0, whm);
    CHECK_EQ(*t.PerHour(300.0), 300.0 * 3600.0 / 100.0);
}

TEST(a_gap_between_updates_counts_for_nothing)
{
    ExpTracker t;
    t.Update(0.0, Sample(30, 1000, 7600), kMe);
    t.Update(1.0, Sample(30, 1100, 7600), kMe);
    t.Update(2.0, Sample(30, 1200, 7600), kMe);
    CHECK_EQ(*t.PerHour(60.0), 200.0 * 3600.0 / 60.0); // gains come a second apart
    t.Update(500.0, Sample(30, 5200, 7600), kMe);      // the widget was off: 4,000 EXP are not a spike
    CHECK(!t.PerHour(500.0).has_value());
    CHECK(!t.PerHour(570.0).has_value());
    t.Update(501.0, Sample(30, 5300, 7600), kMe);
    CHECK_EQ(*t.PerHour(560.0), 100.0 * 3600.0 / 60.0);
}

TEST(another_character_on_the_same_job_and_level_starts_the_rate_again)
{
    ExpTracker t;
    t.Update(0.0, Sample(30, 1000, 7600), kMe);
    t.Update(1.0, Sample(30, 1500, 7600), kMe);
    t.Update(2.0, Sample(30, 6000, 7600), kAnother);
    CHECK(!t.PerHour(70.0).has_value());
}

TEST(a_reset_forgets_the_last_sample)
{
    ExpTracker t;
    t.Update(0.0, Sample(30, 1000, 7600), kMe);
    t.Reset();
    t.Update(1.0, Sample(30, 5000, 7600), kMe);
    CHECK(!t.PerHour(70.0).has_value());
}

TEST(a_merit_arriving_after_its_limit_points_wrapped_is_not_a_gain)
{
    CHECK_EQ(Gained(AtCap(3, 200), AtCap(4, 200)), 0u);
}

TEST(reaching_the_cap_counts_the_last_levels_exp)
{
    JobSample cap = Sample(kLevelCap, 100, 0);
    CHECK_EQ(Gained(Sample(74, 5000, 7600), cap), 2700u);
}

TEST(the_cap_counts_limit_points_with_limit_mode_off)
{
    JobSample off = AtCap(1, 100);
    off.limitMode = false;
    JobSample later = off;
    later.limitPoints = 600;
    CHECK_EQ(Gained(off, later), 500u);
}

TEST(exp_per_hour_is_taken_at_the_last_whole_second_so_its_figure_changes_once_a_second)
{
    ExpTracker t;
    Run(t, 0.0, 29.0, Sample(30, 1000, 7600));
    Run(t, 30.0, 130.0, Sample(30, 1300, 7600));
    for (const double now : {130.0, 130.25, 130.5, 130.99})
        CHECK_EQ(*t.PerHour(now), 300.0 * 3600.0 / 130.0);
    CHECK_EQ(*t.PerHour(131.0), 300.0 * 3600.0 / 131.0);
    // A gain inside the second counts from the next one, and the gain at 30 s leaves the window a whole second late.
    t.Update(130.5, Sample(30, 1400, 7600), kMe);
    CHECK_EQ(*t.PerHour(130.75), 300.0 * 3600.0 / 130.0);
    CHECK_EQ(*t.PerHour(131.0), 400.0 * 3600.0 / 131.0);
    Run(t, 131.0, 630.0, Sample(30, 1400, 7600));
    t.Update(630.5, Sample(30, 1400, 7600), kMe);
    CHECK_EQ(*t.PerHour(630.5), 400.0 * 3600.0 / 600.0);
}

TEST(the_exp_says_what_it_is_set_to_earned_to_next_and_a_percent_in_that_order)
{
    const auto bits = [](std::initializer_list<ExpText> parts) {
        uint8_t b = 0;
        for (const ExpText part : parts)
            b |= ExpTextBit(part);
        return b;
    };
    const JobReport level30{Sample(30, 12345, 20000), std::nullopt};
    const auto title = [](const WidgetContent& w) { return w.title.front().text; };
    CHECK_EQ(title(JobWidget(level30, ExpBarPlace::Under, true, JobLayout::ExpFirst)), "7,655 EXP to next"); // as before
    CHECK_EQ(title(JobWidget(level30, ExpBarPlace::Under, true, JobLayout::ExpFirst, bits({ExpText::Percent}))), "61%");
    CHECK_EQ(title(JobWidget(level30, ExpBarPlace::Under, true, JobLayout::ExpFirst,
                 bits({ExpText::Percent, ExpText::Earned, ExpText::ToNext}))),
        "12,345 / 20,000 EXP · 7,655 to next · 61%");
    // At the cap the points are limit points, after the merits.
    const JobReport capped{AtCap(3, 4000), std::nullopt};
    CHECK_EQ(title(JobWidget(capped, ExpBarPlace::Under, true, JobLayout::ExpFirst, bits({ExpText::Earned, ExpText::Percent}))),
        "3 merits · 4,000 / 10,000 LP · 40%");
    // To Next at the cap: the merits say how far along you are, as they always have.
    CHECK_EQ(title(JobWidget(capped, ExpBarPlace::Under, true, JobLayout::ExpFirst, bits({ExpText::ToNext}))), "3 merits");
    // A percent is never rounded up to a level not reached.
    CHECK_EQ(title(JobWidget(JobReport{Sample(30, 19999, 20000), std::nullopt}, ExpBarPlace::Under, true,
                 JobLayout::ExpFirst, bits({ExpText::Percent}))),
        "99%");
    // Nothing picked: the bar's name alone.
    CHECK_EQ(title(JobWidget(level30, ExpBarPlace::Under, true, JobLayout::ExpFirst, 0)), "EXP");
}

TEST(the_exp_widget_is_its_bar_with_each_text_on_the_side_it_is_set_to)
{
    const auto said = [](const std::vector<Span>& spans) {
        std::string text;
        for (const Span& s : spans)
            text += s.text;
        return text;
    };
    const JobReport level30{Sample(30, 12345, 20000), std::nullopt};
    const WidgetContent w =
        ExpWidget(level30, {ExpTextPlace::Above, ExpTextPlace::Right, ExpTextPlace::Right}, 150, 6);
    CHECK(w.meter && w.title.empty() && w.hasPopup);
    CHECK(std::fabs(w.meter->done - 12345.0f / 20000.0f) < 1e-6f && w.meter->width == 150 && w.meter->height == 6);
    CHECK_EQ(said(w.meter->above), "12,345 / 20,000 EXP");
    CHECK_EQ(said(w.meter->right), "7,655 EXP to next · 61%"); // two on a side say it as the Job widget does
    CHECK(w.meter->left.empty() && w.meter->below.empty());
    // At the cap the bar fills with limit points, and To Next says the merits.
    const WidgetContent capped =
        ExpWidget(JobReport{AtCap(3, 4000), std::nullopt}, {ExpTextPlace::Left, ExpTextPlace::Below, ExpTextPlace::Off}, 150, 6);
    CHECK(capped.meter && std::fabs(capped.meter->done - 0.4f) < 1e-6f);
    CHECK_EQ(said(capped.meter->left), "4,000 / 10,000 LP");
    CHECK_EQ(said(capped.meter->below), "3 merits");
    CHECK(capped.meter->above.empty() && capped.meter->right.empty());
    // Before the server has sent the jobs, a dash.
    const WidgetContent none = ExpWidget(std::nullopt, kDefaultExpPlaces, 150, 6);
    CHECK(!none.meter && none.title.front().text == "-");
}
