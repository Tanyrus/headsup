#include "bar_party.h"

#include "bar_job.h"
#include "zones.h"

#include <algorithm>
#include <iterator>

namespace headsup
{
    namespace
    {
        // By job number, NON (0) to RUN (22): grades.yaml's mp grade is not none.
        constexpr bool kHasMp[] = {false, false, false, true, true, true, false, true, true, false, false, false,
            false, false, false, true, true, false, false, false, true, true, true};
        constexpr int kColumns        = 5;
        constexpr uint8_t kLowPercent = 50, kCriticalPercent = 25;

        // A figure in its percent's color: red under 25 %, amber under 50 %.
        Span Level(std::string text, uint8_t percent)
        {
            if (percent < kCriticalPercent) return Colored(std::move(text), kAlarmColor);
            if (percent < kLowPercent) return Colored(std::move(text), kWarningColor);
            return Text(std::move(text));
        }

        // Listed by the game before its figures arrive: no job yet.
        bool NoData(const PartyMember& m) { return m.mainJob == 0; }

        std::string Percent(uint8_t percent) { return std::to_string(percent) + "%"; }

        int Members(const Party& party)
        {
            return static_cast<int>(
                std::count_if(party.begin(), party.end(), [](const auto& m) { return m.has_value(); }));
        }
    }

    bool JobHasMp(uint8_t job) { return job < std::size(kHasMp) && kHasMp[job]; }

    bool UsesMp(const PartyMember& m) { return JobHasMp(m.mainJob) || (m.subLevel > 0 && JobHasMp(m.subJob)); }

    WidgetContent PartyWidget(const Party& party)
    {
        WidgetContent w;
        w.shown         = true;
        const int count = Members(party);
        if (count <= 1)
        {
            w.title = ListOf(Text("Solo"));
            return w;
        }
        w.title = ListOf(Text("Party " + std::to_string(count)));
        std::optional<uint8_t> hp, mp;
        for (const std::optional<PartyMember>& m : party)
        {
            if (!m || !m->here || NoData(*m)) continue;
            hp = std::min(hp.value_or(m->hpPercent), m->hpPercent);
            if (UsesMp(*m)) mp = std::min(mp.value_or(m->mpPercent), m->mpPercent);
        }
        if (hp) w.subtitle = ListOf(Text("HP ", Tone::Dim), Level(Percent(*hp), *hp));
        if (mp)
        {
            w.subtitle.push_back(Text(kDot, Tone::Dim));
            w.subtitle.push_back(Text("MP ", Tone::Dim));
            w.subtitle.push_back(Level(Percent(*mp), *mp));
        }
        w.hasPopup = true;
        w.figures  = true;
        return w;
    }

    PopupContent PartyPopup(const Party& party)
    {
        PopupContent popup;
        popup.title   = ListOf(Text("Party"));
        PopupRow grid = Grid(kColumns, kColumns * (kPartySize + 1));
        grid.cells.emplace_back();
        grid.cells.emplace_back();
        for (const char* heading : {"HP", "MP", "TP"})
            grid.cells.push_back(Cell(Text(heading, Tone::Dim)));
        std::vector<PopupRow> away; // a member elsewhere: one row of name and zone, after the grid
        for (const std::optional<PartyMember>& m : party)
        {
            if (!m) continue;
            if (!m->here)
            {
                // Zone 0: changing zones, or their data has not arrived.
                const std::string where = m->zone == 0          ? "Zoning"
                                          : KnownZone(m->zone) ? "In " + ZoneTitle(m->zone)
                                                               : ZoneTitle(m->zone);
                PopupRow row{RowKind::Line, 0, {}};
                row.cells = ListOf(Cell(Text(m->name)), Cell(Text(where, Tone::Dim)));
                away.push_back(std::move(row));
                continue;
            }
            grid.cells.push_back(Cell(Text(m->name)));
            if (NoData(*m))
            {
                for (int c = 1; c < kColumns; ++c)
                    grid.cells.push_back(Cell(Text("-", Tone::Dim)));
                continue;
            }
            grid.cells.push_back(Cell(Text(JobsText(m->mainJob, m->mainLevel, m->subJob, m->subLevel), Tone::Dim)));
            grid.cells.push_back(Cell(Level(Grouped(m->hp), m->hpPercent)));
            grid.cells.push_back(Cell(UsesMp(*m) ? Level(Grouped(m->mp), m->mpPercent) : Text("-", Tone::Dim)));
            grid.cells.push_back(
                Cell(m->tp >= kWeaponSkillTp ? Colored(Grouped(m->tp), kReadyColor) : Text(Grouped(m->tp))));
        }
        PopupSection rows{"", ListOf(std::move(grid))};
        for (PopupRow& row : away)
            rows.rows.push_back(std::move(row));
        popup.sections = ListOf(std::move(rows));
        return popup;
    }
}
