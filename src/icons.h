#pragma once

#include "generated/icon_ids.h"
#include "magic_chance.h"
#include "mobdata.h"
#include "plate_settings.h"

#include <cstdint>
#include <optional>
#include <string>

namespace headsup
{
    // The longest row: a mob's aggro, link, sight or true sight, sound, scent, magic, ability and blood.
    constexpr int kMaxIcons = 8;

    struct IconSet
    {
        Icon icons[kMaxIcons];
        int count = 0;
    };

    // In XIUI's order, only the icons whose DetectionIcon bit is in detectionShown. The aggro icon is the mob's nature
    // (whether it aggros at all), not whether it would attack this player.
    IconSet IconsFor(const MobRecord* mob, uint32_t detectionShown);
    // The kind of detection an icon IconsFor gives belongs to; any icon no mob's row holds reads as Aggro.
    DetectionIcon DetectionOf(Icon icon);
    // The word a popup's tag says for an icon IconsFor gives: "Aggressive" for either aggro icon, "True sight"; empty
    // for any other icon.
    const char* IconTag(Icon icon);

    enum class BadgeKind : uint8_t
    {
        Weak,       // takes more damage of a type: its percent
        WeakRank,   // an element it is weak to by rank
        Resist,     // takes less damage of a type: its percent
        ResistRank, // an element it resists by rank
        StatusWeak, // a status it is weak to by rank
        StatusRank, // a status it resists by rank
        Immune,
    };
    struct InfoIcon
    {
        Icon icon       = Icon::AggroNQ;
        BadgeKind badge = BadgeKind::Immune;
        int value       = 0; // the percent's size for Weak and Resist, the rank for the ranks, 0 for Immune
        bool operator==(const InfoIcon&) const = default;
    };
    // Every damage type and every immunity with an icon at once.
    constexpr int kMaxInfoIcons = 32;
    struct InfoIconSet
    {
        InfoIcon icons[kMaxInfoIcons];
        int count = 0;
    };

    // Phoenix clamps resistance ranks to these. Rank 0 is neutral and never gets a badge.
    constexpr int kMinRank = -3, kMaxRank = 11;
    // Cutoffs past the ends: no weaknesses, or no resistances. The loosest cutoffs stop short of neutral.
    constexpr int kNoWeakRank = kMinRank - 1, kNoResistRank = kMaxRank + 1;
    constexpr int kMaxWeakRank = -1, kMinResistRank = 1;

    // A badge shows at or below the weakness cutoff and at or above the resistance cutoff.
    struct StatusBadgeRule
    {
        bool show      = true;
        int weakRank   = kNoWeakRank;
        int resistRank = 4;
        bool operator==(const StatusBadgeRule&) const = default;
    };
    struct BadgeRules
    {
        int elementWeakRank = -1, elementResistRank = 4;
        int maxElements = kElementCount; // by rank; past it the smallest differences from neutral go first
        StatusBadgeRule status[kStatusRankCount] = {};
    };

    // What the mob is weak to, resists and is immune to, in the badge group's order: SDT weaknesses largest first,
    // elements weak by rank weakest first, SDT resistances strongest first, elements resisted by rank highest first,
    // statuses weak by rank weakest first, statuses resisted by rank highest first, then immunities in MobDB's order. An
    // element's rank counts only when its SDT shows nothing; a status the mob is immune to shows no rank. Light and dark
    // sleep share one Sleep badge when theirs are the same.
    InfoIconSet InfoIconsFor(const MobResistances* mob, const BadgeRules& rules);
    enum class BadgeGroup : uint8_t { Weak, Resist, Status, Immune };
    // The entries of InfoIconsFor in one group, in its order: Weak holds Weak and WeakRank, Resist Resist and
    // ResistRank, Status StatusWeak and StatusRank.
    InfoIconSet InfoIconsOf(const MobResistances* mob, const BadgeRules& rules, BadgeGroup group);
    // A change in damage taken or in a chance as the badges spell it: "+25%", "-50%", "0%".
    std::string PercentText(int percent);
    // The mark of an immunity: a times sign.
    constexpr const char* kTimesSign = "\xC3\x97";

    // What a rank's number is figured with: what it says, your accuracy and the mob at the level it is figured for.
    struct RankNumbers
    {
        RankDisplay display  = RankDisplay::IncreasedChance;
        bool countAccuracy   = true;
        const Caster* caster = nullptr;
        std::optional<MagicTarget> target;
    };
    // The mob at the level a check gave (its level mod taken back off) or else the top of its range, when the data has
    // its stats there.
    std::optional<MagicTarget> MagicTargetOf(const MobRecord& mob, int checkedLevel);
    // An element's rank and a status's as RankNumberText gives them, by the spell each stands for.
    std::string ElementRankText(Element element, int rank, const RankNumbers& numbers);
    std::string StatusRankText(StatusRank status, int rank, const RankNumbers& numbers);
    // "+25%", "-50%", a rank's number, or kTimesSign for an immunity.
    std::string BadgeText(const InfoIcon& icon, const RankNumbers& numbers);

    constexpr int kFirstElementSdt = static_cast<int>(Sdt::Fire);
    // A damage type's or element's icon, by its place in Sdt.
    Icon SdtIcon(int sdt);
    // Damage taken as the percent a badge shows: an sdt of 2500 is +25%.
    int SdtPercent(int16_t sdt);
    // A status's icon and MobImmunity bit, by its place in StatusRank.
    Icon StatusIcon(int status);
    uint32_t StatusImmunity(int status);

    // An icon's pixels, rows top to bottom, each B, G, R, A as D3DFMT_A8R8G8B8 keeps it in memory.
    struct IconBitmap
    {
        const unsigned char* bgra;
        uint32_t width;
        uint32_t height;
    };

    const IconBitmap& IconImage(Icon icon);
}
