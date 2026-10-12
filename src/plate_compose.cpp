#include "plate_compose.h"

#include "bar_job.h"
#include "entity_state.h"
#include "icons.h"
#include "labels.h"
#include "look.h"
#include "plate_template.h"

#include <algorithm>
#include <optional>
#include <string_view>
#include <utility>

namespace headsup
{
    namespace
    {
        constexpr float kPercent       = 100.0f;
        constexpr char kSpace          = ' ';
        constexpr const char* kJobJoin = "/";

        // Follows a chain of syncs to the line whose look is shown; a cycle leaves the line its own style.
        const LineDesign& StyleLine(const KindSettings& kind, int line)
        {
            int at = line;
            for (int hop = 0; hop < kPlateLines; ++hop)
            {
                const int other = kind.lines[at].syncWith;
                if (other < 0 || other >= kPlateLines || other == at) return kind.lines[at];
                at = other;
            }
            return kind.lines[line];
        }

        LineLook LookOf(const LineDesign& d, const EntityState& state, const Settings& s, uint32_t gameArgb)
        {
            LineLook look;
            look.font        = d.font.empty() ? s.defaultFont : d.font;
            look.bold        = d.bold;
            look.textSize    = BasePixels(s, d.textSize);
            look.iconSize    = BasePixels(s, d.iconSize);
            look.color       = ResolveColor(d.color, state, s.palette, gameArgb);
            look.shadow      = static_cast<float>(d.shadow) / kPercent;
            look.shadowColor = ResolveColor(d.shadowColor, state, s.palette, gameArgb);
            look.glow        = static_cast<float>(d.glow) / kPercent;
            look.glowSize    = static_cast<float>(d.glowSize) / kPercent;
            look.glowColor   = ResolveColor(d.glowColor, state, s.palette, gameArgb);
            look.align       = d.align;
            return look;
        }

        bool IsTextTag(Tag tag)
        {
            switch (tag)
            {
            case Tag::Name:
            case Tag::Level:
            case Tag::Con:
            case Tag::Id:
            case Tag::ServerId:
            case Tag::Ph:
            case Tag::Job:
            case Tag::Owner:
                return true;
            default:
                return false;
            }
        }

        PlateItem ItemOf(ItemKind kind)
        {
            PlateItem item;
            item.kind = kind;
            return item;
        }
        IconDraw DrawOf(Icon icon, uint32_t tint = kWhite)
        {
            IconDraw draw;
            draw.icon = icon;
            draw.tint = tint;
            return draw;
        }

        bool IsMob(const ActorInfo& info) { return info.state.kind == EntityKind::Mob; }
        bool IsPlayer(const ActorInfo& info) { return info.state.kind == EntityKind::Player; }

        std::string JobText(const PlayerJobs& jobs)
        {
            std::string text = JobName(jobs.mainJob);
            if (jobs.subJob != 0) text += kJobJoin + std::string(JobName(jobs.subJob));
            return text;
        }

        std::string TagText(Tag tag, const ActorInfo& info, const PlayerState& player)
        {
            const CheckResult* checked = info.checked ? &*info.checked : nullptr;
            switch (tag)
            {
            case Tag::Name:
                return info.name;
            case Tag::Owner:
                return info.ownerName;
            case Tag::Level:
                if (IsMob(info)) return LevelText(info.mob, checked);
                if (IsPlayer(info) && info.jobs && info.jobs->mainJob != 0) return std::to_string(info.jobs->mainLevel);
                return {};
            case Tag::Con:
                return IsMob(info) ? ConText(info.mob, checked, player.level) : std::string();
            case Tag::Id:
                return IsMob(info) ? IdText(info.serverId) : std::string();
            case Tag::ServerId:
                return IsMob(info) ? ServerIdText(info.serverId) : std::string();
            case Tag::Ph:
                return IsMob(info) && info.placeholder ? kPlaceholderMark : std::string();
            case Tag::Job:
                return IsPlayer(info) && info.jobs && info.jobs->mainJob != 0 ? JobText(*info.jobs) : std::string();
            default:
                return {};
            }
        }

        std::optional<Icon> PlayerIconImage(Tag tag)
        {
            switch (tag)
            {
            case Tag::Gm: return Icon::Gm;
            case Tag::Mentor: return Icon::Mentor;
            case Tag::New: return Icon::NewAdventurer;
            case Tag::Sync: return Icon::LevelSync;
            case Tag::Away: return Icon::Away;
            case Tag::Linkshell: return Icon::Linkshell;
            case Tag::Bazaar: return Icon::Bazaar;
            case Tag::Seeking: return Icon::Invite;
            default: return std::nullopt;
            }
        }

        bool PlayerHas(Tag tag, const PlayerStatus& status)
        {
            switch (tag)
            {
            case Tag::Gm: return status.gm;
            case Tag::Mentor: return status.mentor;
            case Tag::New: return status.newAdventurer;
            case Tag::Sync: return status.levelSync;
            case Tag::Away: return status.away;
            case Tag::Linkshell: return status.linkshell;
            case Tag::Bazaar: return status.bazaar;
            case Tag::Seeking: return status.seekingParty;
            default: return false;
            }
        }

        bool IsBadgeTag(Tag tag)
        {
            return tag == Tag::Weak || tag == Tag::Resist || tag == Tag::Status || tag == Tag::Immune;
        }

        BadgeGroup GroupOf(Tag tag)
        {
            switch (tag)
            {
            case Tag::Weak: return BadgeGroup::Weak;
            case Tag::Resist: return BadgeGroup::Resist;
            case Tag::Status: return BadgeGroup::Status;
            default: return BadgeGroup::Immune;
            }
        }

        std::vector<IconDraw> TagIcons(Tag tag, const ActorInfo& info, const Settings& s, std::optional<BadgeRules>& badgeRules)
        {
            std::vector<IconDraw> out;
            switch (tag)
            {
            case Tag::Detect:
            {
                if (!IsMob(info)) return out;
                const IconSet set = IconsFor(info.mob, s.detectionShown);
                for (int i = 0; i < set.count; ++i) out.push_back(DrawOf(set.icons[i]));
                return out;
            }
            case Tag::Weak:
            case Tag::Resist:
            case Tag::Status:
            case Tag::Immune:
            {
                if (!IsMob(info)) return out;
                if (!badgeRules)
                {
                    badgeRules = BadgeRules{.damage = true, .statusRanks = true, .immunities = true,
                        .elementWeakRank = s.elementWeakRank, .elementResistRank = s.elementResistRank,
                        .maxElements = s.maxElements};
                    std::ranges::copy(s.statusBadges, badgeRules->status);
                }
                const InfoIconSet set = InfoIconsOf(info.mob ? &ResistancesOf(*info.mob) : nullptr, *badgeRules, GroupOf(tag));
                for (int i = 0; i < set.count; ++i)
                {
                    IconDraw draw = DrawOf(set.icons[i].icon);
                    draw.badge      = BadgeText(set.icons[i], s.showRanks);
                    draw.badgeColor = BadgeColor(set.icons[i].badge, s);
                    out.push_back(std::move(draw));
                }
                return out;
            }
            default:
                break;
            }
            const std::optional<Icon> icon = PlayerIconImage(tag);
            if (!icon || info.state.kind != EntityKind::Player || !info.status || !PlayerHas(tag, *info.status)) return out;
            out.push_back(DrawOf(*icon, tag == Tag::Linkshell ? info.linkshellArgb : kWhite));
            return out;
        }

        // Appends an atom's rendering: letters join the open text run, spaces beside an icon run become a gap.
        struct LineBuilder
        {
            std::vector<PlateItem> items;
            std::string pending;
            bool pendingName = false;
            bool trimNext    = false; // a tag that rendered nothing sat after a space, so the next literal's first space goes
            bool badgeRun    = false; // the last icon run is a badge group's, which wraps apart from other icons

            bool AfterIcons() const { return !items.empty() && items.back().kind == ItemKind::Icons; }
            void Space(size_t count)
            {
                PlateItem item = ItemOf(ItemKind::Space);
                item.spaces    = static_cast<int>(count);
                items.push_back(std::move(item));
            }

            void Empty() { trimNext = pending.ends_with(kSpace); }

            void Literal(std::string_view s)
            {
                if (trimNext && s.starts_with(kSpace)) s.remove_prefix(1);
                trimNext = false;
                pending += s;
            }

            void Text(std::string_view s, bool name = false)
            {
                if (s.empty()) return Empty();
                trimNext = false;
                pending += s;
                pendingName = pendingName || name;
            }

            void Flush(bool iconFollows)
            {
                const size_t first = pending.find_first_not_of(kSpace);
                if (first == std::string::npos)
                {
                    if (!pending.empty() && AfterIcons() && iconFollows) Space(pending.size());
                }
                else
                {
                    const size_t last = pending.find_last_not_of(kSpace);
                    if (first > 0 && AfterIcons()) Space(first);
                    PlateItem item = ItemOf(ItemKind::Text);
                    item.text      = pending.substr(first, last - first + 1);
                    item.name      = pendingName;
                    items.push_back(std::move(item));
                    if (last + 1 < pending.size() && iconFollows) Space(pending.size() - last - 1);
                }
                pending.clear();
                pendingName = false;
            }

            void Icons(std::vector<IconDraw> v, bool badges)
            {
                if (v.empty()) return Empty();
                trimNext = false;
                Flush(true);
                if (AfterIcons() && badgeRun == badges)
                    items.back().icons.insert(items.back().icons.end(), v.begin(), v.end());
                else
                {
                    PlateItem item = ItemOf(ItemKind::Icons);
                    item.icons     = std::move(v);
                    items.push_back(std::move(item));
                }
                badgeRun = badges;
            }

            void Finish() { Flush(false); }
        };
    }

    float BasePixels(const Settings& s, int percent)
    {
        return static_cast<float>(s.baseSize) * static_cast<float>(percent) / kPercent;
    }

    HookAction HookActionFor(const EntityState& state, const Settings& s, bool screenshot)
    {
        if (screenshot) return HookAction::Hide;
        if (!s.enabled || !s.nameplates) return HookAction::Keep;
        switch (s.Kind(PlateKindOf(state)).rows[RowOf(state)].plate)
        {
        case PlateMode::Vanilla: return HookAction::Keep;
        case PlateMode::Hidden: return HookAction::Hide;
        case PlateMode::WhenTargeted: return Targeted(state.target) ? HookAction::Replace : HookAction::Hide;
        default: return HookAction::Replace;
        }
    }

    ComposedPlate ComposePlate(const ActorInfo& info, const PlayerState& player, const Settings& s, uint32_t gameArgb)
    {
        ComposedPlate plate;
        const EntityState& st = info.state;
        if (!s.enabled || !s.nameplates) return plate;
        const KindSettings& kind = s.Kind(PlateKindOf(st));
        const PlateMode mode     = kind.rows[RowOf(st)].plate;
        if (mode == PlateMode::Hidden || mode == PlateMode::Vanilla) return plate;
        if (mode == PlateMode::WhenTargeted && !Targeted(st.target)) return plate;
        const bool nameOnly = (mode == PlateMode::Expanding && !Targeted(st.target)) || (st.kind == EntityKind::Mob && !st.alive);
        std::optional<BadgeRules> badgeRules;
        plate.lines.reserve(kPlateLines);
        for (int i = 0; i < kPlateLines; ++i)
        {
            const LineDesign& style = StyleLine(kind, i);
            if (!LineShows(style.show, st)) continue;
            LineBuilder builder;
            for (const Atom& atom : ParseTemplate(kind.lines[i].text))
            {
                if (nameOnly && !(atom.isTag && atom.tag == Tag::Name)) continue;
                if (!atom.isTag)
                    builder.Literal(atom.text);
                else if (IsTextTag(atom.tag))
                    builder.Text(TagText(atom.tag, info, player), atom.tag == Tag::Name);
                else
                    builder.Icons(TagIcons(atom.tag, info, s, badgeRules), IsBadgeTag(atom.tag));
            }
            builder.Finish();
            if (builder.items.empty()) continue;
            plate.lines.push_back(ComposedLine{i, std::move(builder.items), LookOf(style, st, s, gameArgb)});
        }
        if (kind.ornament.on)
        {
            const int upper = kind.ornament.place == OrnamentPlace::BetweenLines1And2 ? 0 : 1;
            const auto at   = [&](int source) {
                for (size_t i = 0; i < plate.lines.size(); ++i)
                    if (plate.lines[i].source == source) return static_cast<int>(i);
                return kNoOrnament;
            };
            if (at(upper) != kNoOrnament && at(upper + 1) != kNoOrnament)
            {
                plate.ornamentAbove = at(upper + 1);
                plate.ornamentColor = ResolveColor(kind.ornament.color, st, s.palette, gameArgb);
            }
        }
        return plate;
    }

    namespace
    {
        // A line left without items is no line, and the ornament goes with either line it sat between.
        void DropEmptyLines(ComposedPlate& plate)
        {
            const auto empty = [](const ComposedLine& line) { return line.items.empty(); };
            if (const int above = plate.ornamentAbove; above > 0)
            {
                const auto lower    = plate.lines.begin() + above;
                const auto gone     = static_cast<int>(std::count_if(plate.lines.begin(), lower, empty));
                plate.ornamentAbove = empty(*lower) || empty(*(lower - 1)) ? kNoOrnament : above - gone;
            }
            std::erase_if(plate.lines, empty);
        }
    }

    void DropIcons(ComposedPlate& plate)
    {
        for (ComposedLine& line : plate.lines)
            std::erase_if(line.items, [](const PlateItem& item) { return item.kind != ItemKind::Text; });
        DropEmptyLines(plate);
    }

    void DropTexts(ComposedPlate& plate, std::span<const uint8_t> leftOut)
    {
        size_t text = 0;
        for (ComposedLine& line : plate.lines)
        {
            std::vector<PlateItem>& items = line.items;
            size_t kept                   = 0;
            for (size_t i = 0; i < items.size(); ++i)
            {
                const bool gone = items[i].kind == ItemKind::Text && leftOut[text++] != 0;
                // A gap sets two items apart: at a line's start, or after another gap, it has nothing to.
                const bool loose = items[i].kind == ItemKind::Space && (kept == 0 || items[kept - 1].kind == ItemKind::Space);
                if (gone || loose) continue;
                if (kept != i) items[kept] = std::move(items[i]);
                ++kept;
            }
            if (kept > 0 && items[kept - 1].kind == ItemKind::Space) --kept;
            items.resize(kept);
        }
        DropEmptyLines(plate);
    }

    LineLook TimerLook(const Settings& s)
    {
        LineLook look = LookOf(LineDesign{}, EntityState{}, s, kWhite);
        look.textSize = BasePixels(s, s.timerPercent);
        return look;
    }

    void JoinTexts(const ComposedLine& line, std::string& text)
    {
        text.clear();
        for (const PlateItem& item : line.items)
        {
            if (item.kind != ItemKind::Text) continue;
            if (!text.empty()) text += kSpace;
            text += item.text;
        }
    }
}
