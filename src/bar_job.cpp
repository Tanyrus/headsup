#include "bar_job.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <utility>
#include <vector>

namespace headsup
{
    namespace
    {
        const char* const kJobNames[] = {"", "WAR", "MNK", "WHM", "BLM", "RDM", "THF", "PLD", "DRK", "BST", "BRD", "RNG",
            "SAM", "NIN", "DRG", "SMN", "BLU", "COR", "PUP", "DNC", "SCH", "GEO", "RUN"};
        constexpr auto kHour = static_cast<double>(kSecondsPerHour);

        uint32_t Rise(uint64_t before, uint64_t after) { return after > before ? static_cast<uint32_t>(after - before) : 0; }

        uint64_t LimitTotal(const JobSample& s) { return static_cast<uint64_t>(s.merits) * kLimitPointsPerMerit + s.limitPoints; }

        bool AtCap(const JobSample& s) { return s.mainLevel >= kLevelCap; }

        std::string OutOf(uint32_t value, uint32_t whole) { return Grouped(value) + " / " + Grouped(whole); }

        Span PerHourText(const std::optional<double>& perHour)
        {
            return perHour ? Text(Grouped(static_cast<uint32_t>(std::lround(*perHour)))) : Text("-", Tone::Dim);
        }

        std::string Jobs(const JobSample& s) { return JobsText(s.mainJob, s.mainLevel, s.subJob, s.subLevel); }

        uint32_t ToNext(const JobSample& s) { return s.expNeeded > s.exp ? s.expNeeded - s.exp : 0; }

        constexpr int kWholePercent    = 100;

        // The parts picked of points earned out of whole: the unit (EXP or LP) after the first number, and a percent
        // never above what is earned.
        std::string ExpParts(uint32_t earned, uint32_t whole, const char* unit, uint8_t parts)
        {
            std::vector<std::string> said;
            bool named = false;
            const auto unitOnce = [&]() { return std::exchange(named, true) ? std::string() : std::string(" ") + unit; };
            if (parts & ExpTextBit(ExpText::Earned)) said.push_back(OutOf(earned, whole) + unitOnce());
            if (parts & ExpTextBit(ExpText::ToNext))
                said.push_back(Grouped(whole > earned ? whole - earned : 0) + unitOnce() + " to next");
            if (parts & ExpTextBit(ExpText::Percent))
                said.push_back(std::to_string(whole > 0 ? static_cast<uint64_t>(std::min(earned, whole)) * kWholePercent / whole : 0) + "%");
            std::string text;
            for (const std::string& part : said)
                text += (text.empty() ? "" : kDot) + part;
            return text;
        }

        // What parts say of the EXP; at the cap the merits stand for what is left to the next, and the rest are limit
        // points.
        std::string ExpSaid(const JobSample& s, uint8_t parts)
        {
            if (!AtCap(s)) return ExpParts(s.exp, s.expNeeded, "EXP", parts);
            const std::string points = ExpParts(std::min(s.limitPoints, kLimitPointsPerMerit), kLimitPointsPerMerit, "LP",
                static_cast<uint8_t>(parts & ~ExpTextBit(ExpText::ToNext)));
            if (!(parts & ExpTextBit(ExpText::ToNext))) return points;
            return std::to_string(s.merits) + (s.merits == 1 ? " merit" : " merits") + (points.empty() ? "" : kDot + points);
        }

        // How full the bar is: of the level, or of the next merit at the cap; nothing without a level's worth.
        std::optional<float> ExpDone(const JobSample& s)
        {
            if (AtCap(s)) return static_cast<float>(std::min(s.limitPoints, kLimitPointsPerMerit)) / kLimitPointsPerMerit;
            if (s.expNeeded == 0) return std::nullopt;
            return static_cast<float>(std::min(s.exp, s.expNeeded)) / static_cast<float>(s.expNeeded);
        }
    }

    const char* JobName(uint8_t job) { return job < std::size(kJobNames) ? kJobNames[job] : ""; }

    std::string JobsText(uint8_t mainJob, uint8_t mainLevel, uint8_t subJob, uint8_t subLevel)
    {
        const std::string main = JobName(mainJob);
        if (main.empty()) return "-";
        std::string text = main + " " + std::to_string(mainLevel);
        if (const std::string sub = JobName(subJob); !sub.empty() && subLevel > 0)
            text += " / " + sub + " " + std::to_string(subLevel);
        return text;
    }

    uint32_t Gained(const JobSample& before, const JobSample& after)
    {
        if (before.mainJob != after.mainJob) return 0;
        if (AtCap(before) && AtCap(after))
        {
            // The server sends merits and limit points apart, so a merit can arrive a frame after its points wrapped.
            const uint32_t points = Rise(LimitTotal(before), LimitTotal(after));
            return (points < kLimitPointsPerMerit ? points : 0) + Rise(before.exp, after.exp);
        }
        if (after.mainLevel == before.mainLevel) return Rise(before.exp, after.exp);
        if (after.mainLevel == before.mainLevel + 1 && before.expNeeded >= before.exp)
            return before.expNeeded - before.exp + after.exp;
        return 0;
    }

    void ExpTracker::Reset() { m_Last.reset(); }

    void ExpTracker::Update(double now, const JobSample& sample, uint32_t who)
    {
        if (!m_Last || m_Last->mainJob != sample.mainJob || m_Who != who || now - m_LastAt > kExpMaxGap ||
            now < m_LastAt)
        {
            m_Gains.clear();
            m_Since = now;
        }
        else if (const uint32_t points = Gained(*m_Last, sample); points > 0)
            m_Gains.push_back(Gain{now, points});
        m_Last   = sample;
        m_Who    = who;
        m_LastAt = now;
        while (!m_Gains.empty() && m_Gains.front().at < std::floor(now) - kExpWindow)
            m_Gains.pop_front();
    }

    std::optional<double> ExpTracker::PerHour(double now) const
    {
        const double second = std::floor(now);
        const double start  = std::max(m_Since, second - kExpWindow);
        double total        = 0.0;
        for (const Gain& g : m_Gains)
            if (g.at >= start && g.at <= second) total += g.points;
        const double span = second - start;
        if (!m_Last || span < kExpLeastSpan || total <= 0.0) return std::nullopt;
        return total * kHour / span;
    }

    WidgetContent JobWidget(const std::optional<JobReport>& job, ExpBarPlace bar, bool jobNames, JobLayout layout, uint8_t expText)
    {
        WidgetContent w;
        w.shown = true;
        if (!job)
        {
            w.title = ListOf(Text("-"));
            return w;
        }
        const JobSample& s = job->sample;
        Span jobs = Text(jobNames ? Jobs(s)
                                  : "Lv " + std::to_string(s.mainLevel) +
                                        (s.subLevel > 0 && !std::string(JobName(s.subJob)).empty()
                                                ? " / " + std::to_string(s.subLevel)
                                                : ""));
        // At the cap the merits always show, as they stand for what is left to the next.
        const std::string said = ExpSaid(s, AtCap(s) ? static_cast<uint8_t>(expText | ExpTextBit(ExpText::ToNext)) : expText);
        w.progress = ExpDone(s);
        if (said.empty())
            w.title = ListOf(jobs); // no EXP text: the jobs alone, on one line
        else
        {
            Span exp      = Text(said);
            Span& smaller = layout == JobLayout::JobsFirst ? exp : jobs;
            smaller.tone  = Tone::Dim;
            w.title       = ListOf(layout == JobLayout::JobsFirst ? jobs : exp);
            if (layout != JobLayout::ExpOnly) w.subtitle = ListOf(smaller);
        }
        if (bar == ExpBarPlace::Off) w.progress.reset();
        w.progressBeside = bar == ExpBarPlace::Beside;
        w.hasPopup       = true;
        w.figures        = true;
        return w;
    }

    WidgetContent ExpWidget(const std::optional<JobReport>& job, const ExpPlaces& places, int width, int height, Align align)
    {
        WidgetContent w;
        w.shown = true;
        if (!job)
        {
            w.title = ListOf(Text("-"));
            return w;
        }
        Meter m;
        m.done   = ExpDone(job->sample).value_or(0.0f);
        m.width  = width;
        m.height = height;
        m.align  = align;
        const auto side = [&](ExpTextPlace place, std::vector<Span>& spans) {
            uint8_t parts = 0;
            for (int t = 0; t < kExpTextCount; ++t)
                if (places[static_cast<size_t>(t)] == place) parts |= ExpTextBit(static_cast<ExpText>(t));
            if (const std::string said = ExpSaid(job->sample, parts); !said.empty()) spans = ListOf(Text(said));
        };
        side(ExpTextPlace::Above, m.above);
        side(ExpTextPlace::Below, m.below);
        side(ExpTextPlace::Left, m.left);
        side(ExpTextPlace::Right, m.right);
        w.meter    = std::move(m);
        w.hasPopup = true;
        w.figures  = true;
        return w;
    }

    PopupContent JobPopup(const std::optional<JobReport>& job)
    {
        PopupContent popup;
        if (!job) return popup;
        const JobSample& s = job->sample;
        popup.title = ListOf(Text("Job and EXP")); // the jobs and levels are on the bar, in the detail font
        PopupSection rows;
        if (AtCap(s))
        {
            rows.rows.push_back(Labeled("Merits", Text(std::to_string(s.merits) + " / " + std::to_string(s.meritsMax))));
            rows.rows.push_back(Labeled("Limit points", Text(OutOf(s.limitPoints, kLimitPointsPerMerit))));
            rows.rows.push_back(Labeled("Per hour", PerHourText(job->perHour)));
            if (job->perHour && s.limitMode && s.merits < s.meritsMax)
                rows.rows.push_back(Labeled("Next merit in",
                    Text(SpanText((kLimitPointsPerMerit - std::min(s.limitPoints, kLimitPointsPerMerit)) * kHour /
                                  *job->perHour))));
            if (!s.limitMode) rows.rows.push_back(LineOf("Limit Mode is off, so EXP does not become merits.", Tone::Dim));
        }
        else
        {
            rows.rows.push_back(Labeled("EXP", Text(OutOf(s.exp, s.expNeeded))));
            rows.rows.push_back(Labeled("To next", Text(Grouped(ToNext(s)))));
            rows.rows.push_back(Labeled("Per hour", PerHourText(job->perHour)));
            if (job->perHour)
                rows.rows.push_back(Labeled("Next level in", Text(SpanText(ToNext(s) * kHour / *job->perHour))));
            rows.rows.push_back(Labeled("Limit points", Text(OutOf(s.limitPoints, kLimitPointsPerMerit))));
        }
        popup.sections = ListOf(std::move(rows));
        return popup;
    }
}
