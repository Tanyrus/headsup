#include "icons.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <iterator>
#include <vector>

namespace headsup
{
    namespace
    {
#include "generated/icons.inc"

        constexpr double kSdtPerPercent = 100.0;
        // The wikis' percent for each rank from kMinRank, which a skillchain's damage is multiplied by.
        constexpr int kRankPercent[kMaxRank - kMinRank + 1] = {150, 130, 115, 100, 85, 70, 60, 50, 40, 30, 25, 20, 15, 10, 5};
        constexpr int kNeutralPercent                       = 100;
        constexpr Icon kSdtIcons[kSdtCount] = {Icon::Slashing, Icon::Piercing, Icon::Impact, Icon::H2H, Icon::Fire, Icon::Ice,
            Icon::Wind, Icon::Earth, Icon::Lightning, Icon::Water, Icon::Light, Icon::Dark};
        constexpr int kFirstElementSdt = static_cast<int>(Sdt::Fire);
        struct Status
        {
            Icon icon;
            uint32_t immunity;
        };
        constexpr Status kRankStatuses[kStatusRankCount] = {{Icon::Paralyze, kImmuneParalyze}, {Icon::Bind, kImmuneBind},
            {Icon::Silence, kImmuneSilence}, {Icon::Slow, kImmuneSlow}, {Icon::Poison, kImmunePoison},
            {Icon::LightSleep, kImmuneLightSleep}, {Icon::DarkSleep, kImmuneDarkSleep}, {Icon::Blind, kImmuneBlind},
            {Icon::Stun, kImmuneStun}, {Icon::Gravity, kImmuneGravity}};
        // MobDB's order after sleep, which comes first.
        constexpr Status kImmunities[] = {{Icon::Gravity, kImmuneGravity}, {Icon::Bind, kImmuneBind}, {Icon::Stun, kImmuneStun},
            {Icon::Silence, kImmuneSilence}, {Icon::Paralyze, kImmuneParalyze}, {Icon::Blind, kImmuneBlind},
            {Icon::Slow, kImmuneSlow}, {Icon::Poison, kImmunePoison}, {Icon::Elegy, kImmuneElegy},
            {Icon::Requiem, kImmuneRequiem}, {Icon::Petrify, kImmunePetrify}};
        constexpr uint32_t kBothSleeps = kImmuneLightSleep | kImmuneDarkSleep;

        int RankPercent(int rank) { return kRankPercent[std::clamp(rank, kMinRank, kMaxRank) - kMinRank]; }
        bool Weak(int rank, int cutoff) { return cutoff != kNoWeakRank && rank < 0 && rank <= cutoff; }
        bool Resists(int rank, int cutoff) { return cutoff != kNoResistRank && rank > 0 && rank >= cutoff; }

        void HighestFirst(std::vector<InfoIcon>& icons) { std::ranges::stable_sort(icons, std::greater{}, &InfoIcon::value); }
        void LowestFirst(std::vector<InfoIcon>& icons) { std::ranges::stable_sort(icons, std::less{}, &InfoIcon::value); }

        // Keeps at most `most` of the two lists together, the ranks furthest from neutral first and then a weakness, then
        // the element order; each list keeps its own order.
        void KeepFurthest(std::vector<InfoIcon>& weak, std::vector<InfoIcon>& resist, int most)
        {
            if (std::ssize(weak) + std::ssize(resist) <= most) return;
            struct Pick
            {
                int distance;
                bool resist;
                size_t index;
            };
            std::vector<Pick> picks;
            for (size_t i = 0; i < weak.size(); ++i)
                picks.push_back({std::abs(RankPercent(weak[i].value) - kNeutralPercent), false, i});
            for (size_t i = 0; i < resist.size(); ++i)
                picks.push_back({std::abs(RankPercent(resist[i].value) - kNeutralPercent), true, i});
            std::ranges::stable_sort(picks, std::greater{}, &Pick::distance);
            picks.resize(static_cast<size_t>(std::max(most, 0)));
            std::vector<bool> keepWeak(weak.size()), keepResist(resist.size());
            for (const Pick& pick : picks)
                (pick.resist ? keepResist : keepWeak)[pick.index] = true;
            auto keep = [](std::vector<InfoIcon>& icons, const std::vector<bool>& kept) {
                std::vector<InfoIcon> left;
                for (size_t i = 0; i < icons.size(); ++i)
                    if (kept[i]) left.push_back(icons[i]);
                icons = std::move(left);
            };
            keep(weak, keepWeak);
            keep(resist, keepResist);
        }

        // Light and dark sleep with the same badge become one Sleep badge in light sleep's place.
        void MergeSleeps(std::vector<InfoIcon>& icons)
        {
            const auto light = std::ranges::find(icons, Icon::LightSleep, &InfoIcon::icon);
            const auto dark  = std::ranges::find(icons, Icon::DarkSleep, &InfoIcon::icon);
            if (light == icons.end() || dark == icons.end() || light->value != dark->value) return;
            light->icon = Icon::Sleep;
            icons.erase(dark);
        }
    }

    IconSet IconsFor(const MobRecord* mob)
    {
        IconSet set;
        if (mob == nullptr) return set;
        auto add = [&](Icon icon) { set.icons[set.count++] = icon; };

        const bool notorious = (mob->flags & kMobNotorious) != 0;
        if (IsAggressive(*mob))
            add(notorious ? Icon::AggroHQ : Icon::AggroNQ);
        else
            add(notorious ? Icon::PassiveHQ : Icon::PassiveNQ);
        if (mob->flags & kMobLink) add(Icon::Link);

        const bool trueDetection = (mob->flags & kMobTrueDetection) != 0;
        if ((mob->detects & kDetectSight) && !trueDetection) add(Icon::Sight);
        if (trueDetection) add(Icon::TrueSight);
        if (mob->detects & kDetectHearing) add(Icon::Sound);
        if (mob->detects & kDetectScent) add(Icon::Scent);
        if (mob->detects & kDetectMagic) add(Icon::Magic);
        if (mob->detects & kDetectAbility) add(Icon::Ability);
        if (mob->detects & kDetectLowHp) add(Icon::Blood);
        return set;
    }

    InfoIconSet InfoIconsFor(const MobResistances* mob, const BadgeRules& rules)
    {
        InfoIconSet set;
        if (mob == nullptr) return set;
        std::vector<InfoIcon> weak, weakRank, resist, resistRank, statusWeak, statusResist;
        if (rules.damage)
        {
            for (int i = 0; i < kSdtCount; ++i)
            {
                const int percent = static_cast<int>(std::lround(mob->sdt[i] / kSdtPerPercent));
                if (percent > 0) weak.push_back({kSdtIcons[i], BadgeKind::Weak, percent});
                if (percent < 0) resist.push_back({kSdtIcons[i], BadgeKind::Resist, -percent});
                if (i < kFirstElementSdt || percent != 0) continue;
                const int rank = mob->elementRanks[i - kFirstElementSdt];
                if (Weak(rank, rules.elementWeakRank)) weakRank.push_back({kSdtIcons[i], BadgeKind::WeakRank, rank});
                if (Resists(rank, rules.elementResistRank)) resistRank.push_back({kSdtIcons[i], BadgeKind::ResistRank, rank});
            }
            KeepFurthest(weakRank, resistRank, rules.maxElements);
            HighestFirst(weak);
            LowestFirst(weakRank);
            HighestFirst(resist);
            HighestFirst(resistRank);
        }
        if (rules.statusRanks)
        {
            for (int i = 0; i < kStatusRankCount; ++i)
            {
                const StatusBadgeRule& rule = rules.status[i];
                const int rank              = mob->statusRanks[i];
                if (!rule.show || (mob->immunity & kRankStatuses[i].immunity) != 0) continue;
                if (Weak(rank, rule.weakRank)) statusWeak.push_back({kRankStatuses[i].icon, BadgeKind::StatusWeak, rank});
                if (Resists(rank, rule.resistRank)) statusResist.push_back({kRankStatuses[i].icon, BadgeKind::StatusRank, rank});
            }
            MergeSleeps(statusWeak);
            MergeSleeps(statusResist);
            LowestFirst(statusWeak);
            HighestFirst(statusResist);
        }
        auto add = [&](const InfoIcon& icon) {
            if (set.count < kMaxInfoIcons) set.icons[set.count++] = icon;
        };
        for (const std::vector<InfoIcon>* part : {&weak, &weakRank, &resist, &resistRank, &statusWeak, &statusResist})
            for (const InfoIcon& icon : *part)
                add(icon);
        if (rules.immunities)
        {
            const uint32_t sleeps = mob->immunity & kBothSleeps;
            if (sleeps == kBothSleeps) add({Icon::Sleep, BadgeKind::Immune, 0});
            else if (sleeps == kImmuneLightSleep) add({Icon::LightSleep, BadgeKind::Immune, 0});
            else if (sleeps == kImmuneDarkSleep) add({Icon::DarkSleep, BadgeKind::Immune, 0});
            for (const Status& status : kImmunities)
                if ((mob->immunity & status.immunity) != 0) add({status.icon, BadgeKind::Immune, 0});
        }
        return set;
    }

    std::string RankText(int rank, bool ranks)
    {
        if (ranks) return std::to_string(rank);
        const int change = RankPercent(rank) - kNeutralPercent;
        return (change > 0 ? "+" : "") + std::to_string(change) + "%";
    }

    std::string BadgeText(const InfoIcon& icon, bool ranks)
    {
        switch (icon.badge)
        {
        case BadgeKind::Weak: return "+" + std::to_string(icon.value) + "%";
        case BadgeKind::Resist: return "-" + std::to_string(icon.value) + "%";
        case BadgeKind::Immune: return "\xC3\x97"; // the times sign
        case BadgeKind::WeakRank:
        case BadgeKind::ResistRank:
        case BadgeKind::StatusWeak:
        case BadgeKind::StatusRank: break;
        }
        return RankText(icon.value, ranks);
    }

    const IconBitmap& IconImage(Icon icon)
    {
        return kIcons[static_cast<size_t>(icon)];
    }
}
