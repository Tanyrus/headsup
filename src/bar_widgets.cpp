#include "bar_widgets.h"

#include "argb.h"
#include "bar_game_help.h"
#include "bar_server_time.h"
#include "ph_timers.h"
#include "titles.h"
#include "vanadiel.h"
#include "zones.h"

#include <cmath>
#include <string_view>
#include <utility>

namespace headsup
{
    namespace
    {
        const char* const kContainerNames[kContainerCount] = {"Inventory", "Mog Safe", "Storage", "Temporary",
            "Mog Locker", "Mog Satchel", "Mog Sack", "Mog Case", "Mog Wardrobe", "Mog Safe 2", "Mog Wardrobe 2",
            "Mog Wardrobe 3", "Mog Wardrobe 4", "Mog Wardrobe 5", "Mog Wardrobe 6", "Mog Wardrobe 7", "Mog Wardrobe 8"};
        constexpr uint32_t kElementColors[kElementCount] = {0xFFFF8A3C, 0xFF8FD8FF, kBarGreen, 0xFFD8B25A, 0xFFC58CFF,
            0xFF5B9DFF, 0xFFFFFFFF, 0xFF9A8AA8};
        constexpr int kDamageColumns = 4, kElementColumns = 4, kStatusColumns = 5, kBagColumns = 2;
        constexpr int kForecastDays = 5, kForecastColumns = 4;
        constexpr uint16_t kGilItem  = 0xFFFF;

        Span Percent(int percent)
        {
            return Text(PercentText(percent), percent > 0 ? Tone::Weak : percent < 0 ? Tone::Resist : Tone::Neutral);
        }

        Span Rank(int rank, bool ranks)
        {
            return Text(RankText(rank, ranks), rank < 0 ? Tone::Weak : rank > 0 ? Tone::Resist : Tone::Neutral);
        }

        // The data gives zone 0, the unknown one, a calendar of its own, so only a known zone has odds.
        std::optional<WeatherOdds> OddsOn(const BarState& state, uint32_t vanaDay)
        {
            const ZoneRecord* zone = KnownZone(state.zone);
            return zone != nullptr ? WeatherOn(*zone, vanaDay) : std::nullopt;
        }

        bool IsMob(const ActorInfo* target)
        {
            return target != nullptr && target->kind == EntityKind::Mob && target->mob != nullptr;
        }

        // "WAR 75 / NIN 37", or the game's "? ? ?" for an anonymous player, whose jobs come as 0.
        std::string CheckedJobs(const InspectGeneral& g)
        {
            return g.mainJob == 0 ? "? ? ?" : JobsText(g.mainJob, g.mainLevel, g.subJob, g.subLevel);
        }

        // The tracker fills a mob's level only while it lives; a dead target has none, but its record still names
        // its aggro or passive icon.
        bool HasLevel(const ActorInfo& target) { return target.level.text[0] != '\0'; }

        // Every icon of its nature: aggro first, then linking and how it detects.
        std::vector<Icon> MobIcons(const ActorInfo& target)
        {
            const IconSet icons = IconsFor(target.mob, kAllDetection);
            return std::vector<Icon>(icons.icons, icons.icons + icons.count);
        }

        std::optional<Icon> MobIcon(const ActorInfo& target)
        {
            const std::vector<Icon> icons = MobIcons(target);
            return icons.empty() ? std::nullopt : std::optional<Icon>(icons.front());
        }

        uint32_t NameColor(const ActorInfo& target, const Settings& settings) { return RowColor(target.state, settings); }

        uint32_t ShadeColor(const ActorInfo& target, const Settings& settings)
        {
            return ToArgb(settings.palette.toughness[static_cast<int>(target.level.shade)]);
        }

        // The nameplate's icons as words, so the popup and the plates say the same, then NM.
        PopupRow Tags(const MobRecord& mob)
        {
            const IconSet icons = IconsFor(&mob, kAllDetection);
            PopupRow row{RowKind::Tags, 0, {}};
            row.cells.reserve(static_cast<size_t>(icons.count) + 1);
            for (int i = 0; i < icons.count; ++i)
                row.cells.push_back(Cell(Text(IconTag(icons.icons[i]), Tone::Tag)));
            if (mob.flags & kMobNotorious) row.cells.push_back(Cell(Text("NM", Tone::Tag)));
            return row;
        }

        // Phoenix's Treasure Hunter tiers in per mille: a rate rolls as the highest at or below it (mobdata.h).
        constexpr uint16_t kDropTiers[] = {1000, 240, 150, 100, 50, 10, 5, 1};
        constexpr double kPerMillePerPercent = 10.0;

        uint16_t Tier(uint16_t rate)
        {
            for (const uint16_t tier : kDropTiers)
                if (rate >= tier) return tier;
            return 0;
        }

        // "24%", "0.5%", "1.7%".
        std::string ChanceText(double perMille)
        {
            const double percent = perMille / kPerMillePerPercent;
            return percent == std::floor(percent) ? Printed("%.0f%%", percent) : Printed("%.1f%%", percent);
        }

        // What a mob drops at Treasure Hunter 0, and what can be stolen or despoiled from it: each by the game's
        // name, without the entries a group picks for nothing.
        std::vector<PopupSection> Loot(const MobRecord& mob, const BarState& state)
        {
            PopupSection drops{"Drops", {}}, steal{"Steal", {}}, despoil{"Despoil", {}};
            for (const MobDrop& d : DropsOf(mob))
            {
                const char* name = state.itemName != nullptr ? state.itemName(d.item) : nullptr;
                if (d.item == 0 || name == nullptr || *name == '\0') continue;
                switch (d.kind)
                {
                case DropKind::Drop:
                {
                    double chance = Tier(d.rate);
                    if (d.group != 0)
                    {
                        const MobDropGroup& g = DropGroupOf(mob, d.group);
                        chance = g.weight > 0 ? static_cast<double>(g.rate) * d.rate / g.weight : 0.0;
                    }
                    if (chance > 0.0) drops.rows.push_back(Labeled(name, Text(ChanceText(chance))));
                    break;
                }
                case DropKind::Steal: steal.rows.push_back(LineOf(name)); break;
                case DropKind::Despoil: despoil.rows.push_back(LineOf(name)); break;
                }
            }
            std::vector<PopupSection> loot;
            for (PopupSection* s : {&drops, &steal, &despoil})
                if (!s->rows.empty()) loot.push_back(std::move(*s));
            return loot;
        }

        PopupSection Spawn(const MobRecord& mob, const BarState& state)
        {
            PopupSection spawn{"Spawn", {}};
            spawn.rows.push_back(LineOf(mob.respawn > 0 ? "Respawn " + Countdown(mob.respawn) : "No respawn timer"));
            if (mob.placeholderOf != 0)
            {
                const MobRecord* nm = MobById(mob.placeholderOf);
                spawn.rows.push_back(LineOf(std::string("Placeholder for ") + (nm != nullptr ? nm->name : "an NM")));
                if (state.targetPhSeconds)
                {
                    const double left = *state.targetPhSeconds;
                    spawn.rows.push_back(LineOf(left > 0.0 ? "Back in " + Countdown(left) : "Due now", Tone::Weak));
                }
            }
            if (mob.placeholders > 0)
                spawn.rows.push_back(LineOf(std::to_string(mob.placeholders) +
                    (mob.placeholders == 1 ? " placeholder" : " placeholders") + " in this zone"));
            if (mob.placeholderOf == 0 && mob.placeholders == 0)
                spawn.rows.push_back(LineOf("Not a placeholder", Tone::Dim));
            return spawn;
        }

        PopupContent TargetPopup(const BarState& state, const Settings& settings)
        {
            PopupContent popup;
            const ActorInfo* target = state.target;
            if (target == nullptr)
            {
                popup.title = ListOf(Text("No target", Tone::Dim));
                return popup;
            }
            if (!IsMob(target))
            {
                popup.title = ListOf(Text(target->name));
                if (target->kind == EntityKind::Player && state.targetCheck)
                {
                    const PlayerCheck& c = *state.targetCheck;
                    PopupSection rows;
                    rows.rows.push_back(Labeled("Jobs", Text(CheckedJobs(c.general))));
                    if (c.general.masterLevel > 0)
                        rows.rows.push_back(Labeled("Master level", Text(std::to_string(c.general.masterLevel))));
                    if (!c.general.linkshell.empty())
                        rows.rows.push_back(Labeled("Linkshell", Text(c.general.linkshell)));
                    if (const std::string title = TitleName(c.message.title); !title.empty())
                        rows.rows.push_back(Labeled("Title", Text(title)));
                    else if (c.message.title != 0)
                        rows.rows.push_back(Labeled("Title", Text("#" + std::to_string(c.message.title))));
                    rows.rows.push_back(LineOf("As of your check", Tone::Dim));
                    popup.sections = ListOf(std::move(rows));
                }
                return popup;
            }
            const MobRecord& mob      = *target->mob;
            const MobResistances& res = ResistancesOf(mob);
            popup.icon  = MobIcon(*target);
            popup.title = ListOf(Colored(target->name, NameColor(*target, settings)));
            PopupRow identity{RowKind::Line, 0, {}};
            if (HasLevel(*target))
                identity.cells.push_back(Cell(Colored(target->level.text, ShadeColor(*target, settings))));
            identity.cells.push_back(Cell(Text("ID " + Printed("%03X", target->serverId & kTargetIndexBits), Tone::Dim)));

            PopupRow damage = Grid(kDamageColumns, kFirstElementSdt);
            for (int i = 0; i < kFirstElementSdt; ++i)
                damage.cells.push_back(Cell(Percent(SdtPercent(res.sdt[i])), SdtIcon(i)));

            PopupRow elements = Grid(kElementColumns, kElementCount);
            for (int e = 0; e < kElementCount; ++e)
            {
                const int percent = SdtPercent(res.sdt[kFirstElementSdt + e]);
                elements.cells.push_back(
                    Cell(percent != 0 ? Percent(percent) : Rank(res.elementRanks[e], settings.showRanks),
                        SdtIcon(kFirstElementSdt + e)));
            }

            PopupRow statuses = Grid(kStatusColumns, kStatusRankCount);
            for (int s = 0; s < kStatusRankCount; ++s)
            {
                const bool immuneTo = (res.immunity & StatusImmunity(s)) != 0;
                statuses.cells.push_back(
                    Cell(immuneTo ? Text(kTimesSign, Tone::Immune) : Rank(res.statusRanks[s], settings.showRanks),
                        StatusIcon(s)));
            }

            PopupRow immune{RowKind::Line, 0, {}};
            const InfoIconSet immunities = InfoIconsOf(&res, BadgeRules{}, BadgeGroup::Immune);
            for (int i = 0; i < immunities.count; ++i)
                immune.cells.push_back(PopupCell{immunities.icons[i].icon, true, {}});
            if (immune.cells.empty()) immune.cells.push_back(Cell(Text("None", Tone::Neutral)));

            popup.sections = ListOf(PopupSection{"", ListOf(std::move(identity), Tags(mob))},
                PopupSection{"Damage taken", ListOf(std::move(damage))},
                PopupSection{"Elements", ListOf(std::move(elements))},
                PopupSection{"Statuses", ListOf(std::move(statuses))},
                PopupSection{"Immune", ListOf(std::move(immune))}, Spawn(mob, state));
            for (PopupSection& loot : Loot(mob, state))
                popup.sections.push_back(std::move(loot));
            return popup;
        }

        Span WeekdayText(const VanaTime& t)
        {
            return Colored(WeekdayName(t.weekday), ElementColor(WeekdayElement(t.weekday)));
        }

        std::string MoonText(const VanaTime& t) { return MoonPhaseName(t.moonPercent, t.moonDirection); }

        bool InConquest(const ZoneRecord* zone) { return zone != nullptr && zone->region < kConquestRegionCount; }

        // Who holds the zone's region; no one before a conquest update or outside the conquest regions.
        Nation OwnerOf(const ZoneRecord* zone, const BarState& state)
        {
            return InConquest(zone) && state.owners ? (*state.owners)[zone->region] : Nation::None;
        }

        std::optional<Icon> WeatherIcon(Weather weather)
        {
            const std::optional<Element> element = WeatherElement(weather);
            if (!element) return std::nullopt;
            return SdtIcon(kFirstElementSdt + static_cast<int>(*element));
        }

        Span BagFill(const BagCount& bag)
        {
            return Text(std::to_string(bag.used) + " / " + std::to_string(bag.max),
                bag.used >= bag.max ? Tone::Resist : Tone::Normal);
        }
    }

    uint32_t ElementColor(Element element) { return kElementColors[static_cast<int>(element)]; }

    std::optional<Span> StripBagFill(const BarState& state)
    {
        return state.stripBag ? std::optional<Span>(BagFill(*state.stripBag)) : std::nullopt;
    }

    std::optional<uint32_t> GilOf(uint16_t item, uint32_t count)
    {
        return item == kGilItem ? std::optional<uint32_t>(count) : std::nullopt;
    }

    std::optional<BagCount> BagOf(uint32_t used, uint32_t max)
    {
        return max > 0 ? std::optional<BagCount>(BagCount{used, max}) : std::nullopt;
    }

    std::optional<int> ContainerNamed(const std::string& caption)
    {
        for (int c = 0; c < kContainerCount; ++c)
            if (caption == kContainerNames[c]) return c;
        return std::nullopt;
    }

    BarNeeds NeedsOf(const Settings& settings, std::optional<BarWidget> open)
    {
        auto shown = [&](BarWidget widget) { return settings.barWidgets[static_cast<int>(widget)]; };
        BarNeeds needs;
        // The Clock in a zone of its own goes by the time alone, as its server line and its popup's resets do; in the
        // computer's zone, by what Windows makes of the time.
        const bool clock = shown(BarWidget::EarthTime), zonedClock = clock && settings.clockZone.has_value();
        needs.unixTime   = shown(BarWidget::Weather) || shown(BarWidget::VanaTime) || zonedClock ||
                           (clock && (settings.clockServerTime || open == BarWidget::EarthTime)) ||
                           (shown(BarWidget::Zone) && open == BarWidget::Zone); // the popup counts down to the tally
        needs.localTime  = clock && !zonedClock;
        needs.zone       = shown(BarWidget::Zone) || shown(BarWidget::Weather) ||
                     (shown(BarWidget::Signet) && open == BarWidget::Signet); // the popup says where it works
        needs.target     = shown(BarWidget::Target) || shown(BarWidget::Distance);
        needs.gil        = shown(BarWidget::Gil);
        needs.strip      = shown(BarWidget::GameHelp);
        needs.frames     = shown(BarWidget::Fps);
        needs.job        = shown(BarWidget::Job);
        needs.connection = shown(BarWidget::Connection);
        needs.party      = shown(BarWidget::Party);
        needs.signet     = shown(BarWidget::Signet);
        // Ashita counts a container's items by walking its slots, so the others wait for the popup that lists them.
        if (shown(BarWidget::Bags)) needs.containers = open == BarWidget::Bags ? kContainerCount : 1;
        return needs;
    }

    namespace
    {
        WidgetContent ContentOf(BarWidget widget, const BarState& state, const Settings& settings);
    }

    WidgetContent BarWidgetContent(BarWidget widget, const BarState& state, const Settings& settings)
    {
        // Each widget's own content, less its second line or icons where they are switched off.
        WidgetContent w = ContentOf(widget, state, settings);
        const int i     = static_cast<int>(widget);
        if (!settings.barSecondLine[i])
        {
            w.subtitle.clear();
            w.subtitleIcons.clear();
        }
        if (!settings.barIcons[i])
        {
            w.icons.clear();
            w.titleIcons.clear();
            w.subtitleIcons.clear();
            w.coin = false;
        }
        return w;
    }

    namespace
    {
    WidgetContent ContentOf(BarWidget widget, const BarState& state, const Settings& settings)
    {
        WidgetContent w;
        w.shown = settings.barWidgets[static_cast<int>(widget)];
        if (!w.shown) return w;
        switch (widget)
        {
        case BarWidget::Zone:
        {
            const ZoneRecord* zone = KnownZone(state.zone);
            const std::string_view region = ZoneRegion(zone);
            w.title = ListOf(Text(ZoneTitle(state.zone)));
            // A conquest region always has a name.
            if (const Nation owner = OwnerOf(zone, state); owner != Nation::None)
                w.subtitle = ListOf(Text(std::string(region), Tone::Dim), Text(kDot, Tone::Dim),
                    Colored(NationName(owner), NationColor(owner)));
            else if (!region.empty())
                w.subtitle = ListOf(Text(std::string(region), Tone::Dim));
            w.hasPopup = state.zone != 0;
            break;
        }
        case BarWidget::Weather:
        {
            const std::optional<WeatherOdds> today = OddsOn(state, VanaAt(state.unixMs).day);
            if (!state.weather && !today)
            {
                w.shown = false;
                break;
            }
            const Weather now = state.weather ? *state.weather : today->normal;
            if (const std::optional<Icon> icon = WeatherIcon(now)) w.icons.assign(IsDoubleWeather(now) ? 2 : 1, *icon);
            w.title = ListOf(Text(WeatherName(now)));
            // Today's chance of it; the forecast is in the popup.
            for (const WeatherChance& c : today ? Chances(*today) : WeatherChances{})
                if (c.weather == now) w.subtitle = ListOf(Text(std::to_string(c.percent) + "%", Tone::Dim));
            w.hasPopup = today.has_value();
            break;
        }
        case BarWidget::VanaTime:
        {
            const VanaTime t = VanaAt(state.unixMs);
            w.title    = ListOf(WeekdayText(t), Text(" " + HourMinute(t.hour, t.minute)));
            w.subtitle = ListOf(Text(MoonText(t) + " " + std::to_string(t.moonPercent) + "%", Tone::Dim));
            w.hasPopup = true;
            break;
        }
        case BarWidget::Target:
            if (state.target == nullptr)
                w.title = ListOf(Text("No target", Tone::Dim));
            else if (state.target->kind != EntityKind::Mob)
            {
                w.title = ListOf(Text(state.target->name));
                if (state.target->kind == EntityKind::Player && state.targetCheck)
                {
                    w.subtitle = ListOf(Text(CheckedJobs(state.targetCheck->general), Tone::Dim));
                    w.hasPopup = true;
                }
            }
            else
            {
                if (settings.barTargetDetails)
                    w.titleIcons = MobIcons(*state.target);
                else if (const std::optional<Icon> icon = MobIcon(*state.target))
                    w.icons.push_back(*icon);
                w.title = ListOf(Colored(state.target->name, NameColor(*state.target, settings)));
                if (HasLevel(*state.target))
                    w.subtitle = ListOf(Colored(state.target->level.text, ShadeColor(*state.target, settings)));
                if (settings.barTargetDetails && IsMob(state.target))
                {
                    // Weaknesses by SDT or rank, as the badges find them by default.
                    const InfoIconSet found = InfoIconsFor(&ResistancesOf(*state.target->mob), BadgeRules{});
                    for (int i = 0; i < found.count; ++i)
                        if (found.icons[i].badge == BadgeKind::Weak || found.icons[i].badge == BadgeKind::WeakRank)
                            w.subtitleIcons.push_back(found.icons[i].icon);
                }
                w.hasPopup = IsMob(state.target); // a mob the data lacks gets its name, level and con, no popup
            }
            break;
        case BarWidget::Gil:
        {
            // The picked currency; gil with its coin.
            const int picked = settings.barCurrency;
            if (picked == kGilCurrency)
            {
                w.coin  = true;
                w.title = ListOf(Text(state.gil ? Grouped(*state.gil) : "-"));
            }
            else
            {
                const std::optional<uint32_t>& amount = state.currencies[picked];
                w.title    = ListOf(Text(amount ? Grouped(*amount) : "-"));
                w.subtitle = ListOf(Text(kCurrencyTable[picked].name, Tone::Dim));
            }
            w.hasPopup = true;
            break;
        }
        case BarWidget::Bags:
            w.title    = ListOf(state.bags[0] ? BagFill(*state.bags[0]) : Text("-"));
            w.subtitle = ListOf(Text("Bags", Tone::Dim));
            w.hasPopup = true;
            break;
        case BarWidget::EarthTime:
            w.title    = ListOf(Text(settings.clockZone
                    ? ZoneClockText(ZoneAt(*settings.clockZone, state.unixMs), settings.clock24, settings.clockSeconds)
                    : ClockText(state.earthHour, state.earthMinute, settings.clock24,
                          settings.clockSeconds ? state.earthSecond : -1)));
            if (settings.clockServerTime)
                w.subtitle = ListOf(Text(
                    ZoneClockText(ZoneAt(ClockZone::Japan, state.unixMs), settings.clock24, false), Tone::Dim));
            w.hasPopup = true;
            break;
        case BarWidget::GameHelp:
            return WidgetContent{};
        case BarWidget::Fps:
            return FpsWidget(state.frames);
        case BarWidget::Job:
            return JobWidget(state.job, settings.expBar, settings.jobNames, settings.jobLayout);
        case BarWidget::Connection:
            return ConnectionWidget(state.connection, settings.barLatency);
        case BarWidget::Party:
            return PartyWidget(state.party);
        case BarWidget::Signet:
            return SignetWidget(state.signet);
        case BarWidget::Distance:
            // Hidden without a target, as the game's own distance shows nothing then.
            w.shown = state.targetDistance && std::isfinite(*state.targetDistance);
            if (!w.shown) break;
            w.title   = ListOf(Text(Printed("%.1f", *state.targetDistance)), Text(" yalms", Tone::Dim));
            w.figures = true;
            break;
        }
        return w;
    }

    }

    PopupContent BarPopupContent(BarWidget widget, const BarState& state, const Settings& settings)
    {
        PopupContent popup;
        switch (widget)
        {
        case BarWidget::Zone:
        {
            const ZoneRecord* zone = KnownZone(state.zone);
            const std::string_view region = ZoneRegion(zone);
            popup.title = ListOf(Text(ZoneTitle(state.zone)));
            // A zone past the data has its number for a title already.
            if (zone != nullptr) popup.title.push_back(Text(" " + std::to_string(state.zone), Tone::Dim));
            if (!region.empty())
                popup.sections.push_back(PopupSection{
                    "", ListOf(Line(Cell(ListOf(Text("Region ", Tone::Dim), Text(std::string(region))))))});
            if (InConquest(zone) && state.shares)
            {
                PopupSection influence{"Influence", {}};
                const Nation owner = OwnerOf(zone, state);
                for (const RegionShare& share : *state.shares)
                {
                    PopupRow row = Labeled(Colored(NationName(share.nation), NationColor(share.nation)),
                        Text(std::to_string(share.percent) + "%"));
                    row.cells[0].star = share.nation == owner;
                    influence.rows.push_back(std::move(row));
                }
                influence.rows.push_back(Labeled("Tally in", Text(SpanText(SecondsToTally(state.unixMs)))));
                popup.sections.push_back(std::move(influence));
            }
            break;
        }
        case BarWidget::Weather:
        {
            const VanaTime t = VanaAt(state.unixMs);
            popup.title      = ListOf(Text("Weather"));
            const std::optional<WeatherOdds> today = OddsOn(state, t.day);
            const Weather now = state.weather ? *state.weather : today ? today->normal : Weather::None;
            PopupCell current = Cell(Text(WeatherName(now)), WeatherIcon(now));
            if (!state.weather) current.text.push_back(Text(" (likely)", Tone::Dim));
            PopupRow grid   = Grid(kForecastColumns, kForecastDays * kForecastColumns);
            bool oneWeather = true; // on every day
            for (int d = 0; d < kForecastDays; ++d)
            {
                const int weekday = (t.weekday + d) % kVanaDaysPerWeek;
                grid.cells.push_back(
                    Cell(Colored(d == 0 ? "Today" : WeekdayName(weekday), ElementColor(WeekdayElement(weekday)))));
                const std::optional<WeatherOdds> odds = OddsOn(state, t.day + static_cast<uint32_t>(d));
                const WeatherChances chances = odds ? Chances(*odds) : WeatherChances{};
                oneWeather = oneWeather && chances.count == 1;
                for (int c = 0; c < kForecastColumns - 1; ++c)
                {
                    if (c >= chances.count)
                    {
                        grid.cells.emplace_back();
                        continue;
                    }
                    const WeatherChance& chance = chances.chances[c];
                    grid.cells.push_back(Cell(Text(std::string(WeatherName(chance.weather)) + " " +
                                                   std::to_string(chance.percent) + "%"),
                        WeatherIcon(chance.weather)));
                }
            }
            PopupSection forecast{"Forecast", ListOf(std::move(grid))};
            // One weather on every day (Phoenix's static zones: Promyvion, Temenos): rolling again changes nothing.
            if (!oneWeather)
                forecast.rows.push_back(
                    LineOf("Phoenix rolls the weather again every 3 to 30 Earth minutes.", Tone::Dim));
            popup.sections = ListOf(PopupSection{"", ListOf(Line(std::move(current)))}, std::move(forecast));
            break;
        }
        case BarWidget::VanaTime:
        {
            const VanaTime t = VanaAt(state.unixMs);
            popup.title      = ListOf(Text(std::to_string(t.year) + "/" + Printed("%02d", t.month) + "/" +
                                           Printed("%02d", t.dayOfMonth)));
            popup.sections   = ListOf(PopupSection{"",
                ListOf(Line(Cell(ListOf(WeekdayText(t), Text(" " + HourMinute(t.hour, t.minute))))),
                    LineOf(MoonText(t) + " (" + std::to_string(t.moonPercent) + "%)"),
                    LineOf("Next day in " + Countdown(SecondsToNextDay(state.unixMs)), Tone::Dim),
                    LineOf("Next phase in " + Countdown(SecondsToNextPhase(state.unixMs)), Tone::Dim))});
            break;
        }
        case BarWidget::Target: return TargetPopup(state, settings);
        case BarWidget::Gil:
        {
            // Gil, then every currency the server gave an amount of; those it has none of stay out.
            popup.title = ListOf(Text("Currencies"));
            PopupSection rows;
            rows.rows.push_back(Labeled("Gil", Text(state.gil ? Grouped(*state.gil) : "-")));
            bool sent = false;
            for (int c = kGilCurrency + 1; c < kCurrencyCount; ++c)
            {
                const std::optional<uint32_t>& amount = state.currencies[c];
                sent = sent || amount.has_value();
                if (amount && *amount > 0) rows.rows.push_back(Labeled(kCurrencyTable[c].name, Text(Grouped(*amount))));
            }
            if (!sent) rows.rows.push_back(LineOf("The rest show once the server sends them.", Tone::Dim));
            popup.sections = ListOf(std::move(rows));
            break;
        }
        case BarWidget::Bags:
        {
            popup.title   = ListOf(Text("Bags"));
            PopupRow grid = Grid(kBagColumns, kBagColumns * kContainerCount);
            for (int c = 0; c < kContainerCount; ++c)
            {
                if (!state.bags[c]) continue;
                grid.cells.push_back(Cell(Text(kContainerNames[c])));
                grid.cells.push_back(Cell(BagFill(*state.bags[c])));
            }
            popup.sections = ListOf(PopupSection{"", ListOf(std::move(grid))});
            break;
        }
        case BarWidget::EarthTime:
        {
            int year = state.earthYear, month = state.earthMonth, day = state.earthDay;
            if (settings.clockZone)
            {
                const ZoneTime there = ZoneAt(*settings.clockZone, state.unixMs);
                year = there.year, month = there.month, day = there.day;
            }
            popup.title =
                ListOf(Text(std::to_string(year) + "-" + Printed("%02d", month) + "-" + Printed("%02d", day)));
            popup.sections = ListOf(ServerTimeRows(state.unixMs));
            break;
        }
        case BarWidget::GameHelp:
            return GameHelpPopup(state.strip, StripBagFill(state));
        case BarWidget::Fps:
            return FpsPopup(state.frames);
        case BarWidget::Job:
            return JobPopup(state.job);
        case BarWidget::Connection:
            return ConnectionPopup(state.connection);
        case BarWidget::Party:
            return PartyPopup(state.party);
        case BarWidget::Signet:
            return SignetPopup(state.signet, state.zone);
        case BarWidget::Distance: break;
        }
        return popup;
    }
}
