#pragma once

#include "bar_content.h"
#include "bar_types.h"

#include <cstdint>
#include <deque>
#include <optional>
#include <string>

namespace headsup
{
    constexpr int kLevelCap                 = 75;    // Phoenix's, in the Treasures of Aht Urhgan era
    constexpr uint32_t kLimitPointsPerMerit = 10000; // Phoenix's MAX_LIMIT_POINTS (merit.cpp)
    constexpr double kExpWindow             = 600.0; // seconds of gains EXP per hour is taken over
    constexpr double kExpLeastSpan          = 60.0;  // seconds of them before it is shown
    constexpr double kExpMaxGap             = 5.0;   // seconds without a sample after which the next one is a new start

    // Your jobs and EXP as the game keeps them (Ashita's IPlayer, from the server's 0x061 and 0x063).
    struct JobSample
    {
        uint8_t mainJob = 0, mainLevel = 0, subJob = 0, subLevel = 0;
        uint32_t exp = 0, expNeeded = 0; // into this level, and the whole of it
        uint32_t limitPoints = 0, merits = 0, meritsMax = 0;
        bool limitMode = false;
        bool operator==(const JobSample&) const = default;
    };

    // "WAR" and the rest by job number; "" for none or one past RUN.
    const char* JobName(uint8_t job);
    // "WAR 75 / NIN 37", the subjob left out when it has no name or no levels; "-" without a main job.
    std::string JobsText(uint8_t mainJob, uint8_t mainLevel, uint8_t subJob, uint8_t subLevel);
    // What after earned over before: EXP below the cap; limit points (and any EXP) at it. Nothing across a job change,
    // a lost level, a jump of more than one (a sync lifted) or EXP lost.
    uint32_t Gained(const JobSample& before, const JobSample& after);

    class ExpTracker
    {
    public:
        // who is the player's server id: another character, a job change or a gap longer than kExpMaxGap (the widget
        // off, the title screen) starts the rate again rather than counting the difference as gained.
        void Update(double now, const JobSample& sample, uint32_t who);
        void Reset();
        // Points gained an hour over the kExpWindow up to now's whole second, so the figure shown changes once a second
        // at most; nothing with less than kExpLeastSpan of this job tracked or nothing gained in it.
        std::optional<double> PerHour(double now) const;

    private:
        struct Gain
        {
            double at       = 0.0;
            uint32_t points = 0;
        };
        std::optional<JobSample> m_Last;
        uint32_t m_Who    = 0;
        double m_LastAt   = 0.0;
        double m_Since = 0.0; // when this job's tracking began
        std::deque<Gain> m_Gains;
    };

    struct JobReport
    {
        JobSample sample;
        std::optional<double> perHour;
        bool operator==(const JobReport&) const = default;
    };

    // jobNames off: the levels alone, "Lv 30 / 15". layout: which of the jobs and the EXP is the title, the other
    // under it, or the EXP alone.
    // expText: ExpTextBit of each part the EXP says; none, and it says EXP.
    WidgetContent JobWidget(const std::optional<JobReport>& job, ExpBarPlace bar = ExpBarPlace::Under,
        bool jobNames = true, JobLayout layout = JobLayout::JobsFirst, uint8_t expText = kDefaultExpText);
    PopupContent JobPopup(const std::optional<JobReport>& job);
}
