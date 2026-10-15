#include "plate_settings.h"

#include <algorithm>
#include <cmath>

namespace headsup
{
    namespace
    {
        const char* const kMobRowKeys[kCategoryCount]   = {"aggressive", "passive", "noData", "aggressiveNm", "passiveNm", "placeholder", "partyClaim", "othersClaim", "dead"};
        const char* const kPlayerRowKeys[kPlayerRowCount]   = {"player", "partyMember", "friend", "linkshell"};
        const char* const kPetRowKeys[kPetRowCount]   = {"yourPet", "partyPet", "friendPet", "othersPet"};
        const char* const kOneRowKeys[kPlateKindCount]   = {"", "", "self", "npc", ""};
        const char* const kKindKeys[kPlateKindCount]     = {"mobs", "players", "self", "npcs", "pets"};

        LineDesign NameLine(const char* text)
        {
            LineDesign line;
            line.text         = text;
            line.color.source = ColorSource::Game;
            return line;
        }
    }

    Palette DefaultPalette()
    {
        Palette p;
        const Color aggression[kNatureCount] = {{{0.20f, 1.00f, 0.30f}}, {{1.00f, 0.15f, 0.15f}}, {{1.00f, 0.84f, 0.00f}},
            {{1.00f, 0.60f, 0.10f}}, {{0.70f, 0.30f, 1.00f}}, {{0.70f, 0.70f, 0.70f}}};
        const Color toughness[kLabelShadeCount] = {{{0.60f, 0.60f, 0.60f}}, {{0.60f, 0.60f, 0.60f}}, {{0.40f, 1.00f, 0.40f}},
            {{0.45f, 0.70f, 1.00f}}, {{1.00f, 1.00f, 1.00f}}, {{1.00f, 1.00f, 0.35f}}, {{1.00f, 0.35f, 0.35f}}, {{0.72f, 0.08f, 0.08f}}};
        const Color claim[kClaimStateCount] = {{{1.00f, 0.95f, 0.60f}}, {{0.55f, 0.45f, 0.40f}}, {{0.30f, 0.65f, 1.00f}},
            {{1.00f, 0.60f, 0.00f}}};
        const Color target[kTargetStateCount] = {{{1.00f, 1.00f, 1.00f}}, ColorFromRgb(kPhoenixRed), {{0.65f, 0.40f, 1.00f}},
            {{0.99f, 0.82f, 0.09f}}, {{1.00f, 0.25f, 0.25f}}};
        const Color type[kTargetTypeCount] = {{{1.00f, 1.00f, 1.00f}}, {{1.00f, 1.00f, 1.00f}}, {{1.00f, 0.90f, 0.55f}},
            {{0.55f, 0.80f, 1.00f}}, {{0.70f, 1.00f, 0.70f}}, {{1.00f, 1.00f, 1.00f}}, {{0.70f, 0.95f, 0.80f}}};
        std::ranges::copy(aggression, p.aggression);
        std::ranges::copy(toughness, p.toughness);
        std::ranges::copy(claim, p.claim);
        std::ranges::copy(target, p.target);
        std::ranges::copy(type, p.type);
        return p;
    }

    std::array<KindSettings, kPlateKindCount> DefaultKinds()
    {
        std::array<KindSettings, kPlateKindCount> kinds;
        KindSettings& mobs = kinds[static_cast<int>(PlateKind::Mobs)];
        for (const Category shown : {Category::WillAttack, Category::NmWillAttack, Category::NmWontAttack, Category::Placeholder})
            mobs.rows[CategoryIndex(shown)].outline = OutlineMode::Shown;
        for (const Category claim : {Category::PartyClaim, Category::OtherClaim})
        {
            mobs.rows[CategoryIndex(claim)].outline              = OutlineMode::ByNature;
            mobs.rows[CategoryIndex(claim)].outlineColor.source = ColorSource::Claim;
        }
        mobs.lines[0].text         = kTagDetect;
        mobs.lines[1].text         = kTagLevelCon;
        mobs.lines[1].textSize     = 85;
        mobs.lines[1].color.source = ColorSource::Toughness;
        mobs.lines[2]              = NameLine(kTagName);
        mobs.lines[2].glow         = 100;
        for (LineDesign& line : mobs.lines)
            line.glowOffInCombat = true;
        mobs.ornament.place        = OrnamentPlace::BelowMiddle;
        const LineDesign player = NameLine(kTagPlayerName);
        kinds[static_cast<int>(PlateKind::Players)].lines[2] = player;
        kinds[static_cast<int>(PlateKind::Self)].lines[2]    = player;
        kinds[static_cast<int>(PlateKind::Npcs)].lines[2]    = NameLine(kTagName);
        KindSettings& pets = kinds[static_cast<int>(PlateKind::Pets)];
        pets.lines[2]      = NameLine(kTagName);
        for (int row = 0; row < kPetRowCount; ++row)
            pets.rows[row].plate = PlateMode::WhenTargeted;
        // Only a mob has an aggression, and only the Mobs page offers that palette: the other kinds' picks start on
        // one theirs do.
        for (const PlateKind kind : {PlateKind::Players, PlateKind::Self, PlateKind::Npcs, PlateKind::Pets})
        {
            KindSettings& k = kinds[static_cast<int>(kind)];
            for (RowSettings& row : k.rows)
                row.outlineColor.source = ColorSource::Type;
            for (LineDesign& line : k.lines)
                line.glowColor.source = ColorSource::Type;
            k.ornament.color.source = ColorSource::Type;
        }
        return kinds;
    }

    int RowCountOf(PlateKind kind)
    {
        switch (kind)
        {
        case PlateKind::Mobs: return kCategoryCount;
        case PlateKind::Players: return kPlayerRowCount;
        case PlateKind::Pets: return kPetRowCount;
        default: return 1;
        }
    }

    const char* RowKey(PlateKind kind, int row)
    {
        if (kind == PlateKind::Mobs) return kMobRowKeys[row];
        if (kind == PlateKind::Players) return kPlayerRowKeys[row];
        if (kind == PlateKind::Pets) return kPetRowKeys[row];
        return kOneRowKeys[static_cast<int>(kind)];
    }

    const char* KindKey(PlateKind kind) { return kKindKeys[static_cast<int>(kind)]; }

    std::span<const ShowCondition> ConditionsFor(PlateKind kind)
    {
        static constexpr ShowCondition kMobs[] = {ShowCondition::Generally, ShowCondition::Targeted, ShowCondition::Unclaimed,
            ShowCondition::YourClaim, ShowCondition::OthersClaim, ShowCondition::Engaged, ShowCondition::TooWeak,
            ShowCondition::Aggressive, ShowCondition::Passive};
        static constexpr ShowCondition kPlayers[] = {ShowCondition::Generally, ShowCondition::Targeted,
            ShowCondition::PartyMember, ShowCondition::Friend, ShowCondition::LinkshellMember};
        static constexpr ShowCondition kSelf[] = {ShowCondition::Generally, ShowCondition::Targeted, ShowCondition::Engaged};
        static constexpr ShowCondition kPets[] = {ShowCondition::Generally, ShowCondition::Targeted, ShowCondition::OwnerIsYou};
        static constexpr ShowCondition kOthers[] = {ShowCondition::Generally, ShowCondition::Targeted};
        switch (kind)
        {
        case PlateKind::Mobs: return kMobs;
        case PlateKind::Players: return kPlayers;
        case PlateKind::Self: return kSelf;
        case PlateKind::Pets: return kPets;
        default: return kOthers;
        }
    }

    std::vector<ColorSource> SourcesFor(PlateKind kind, bool text)
    {
        std::vector<ColorSource> sources{ColorSource::Custom};
        if (kind == PlateKind::Mobs)
            sources.insert(sources.end(), {ColorSource::Aggression, ColorSource::Toughness, ColorSource::Claim});
        sources.insert(sources.end(), {ColorSource::Target, ColorSource::Type});
        if (text) sources.push_back(ColorSource::Game);
        return sources;
    }

    std::vector<ColorSource> CursorSources() { return SourcesFor(PlateKind::Mobs, false); }

    int OutlineModesFor(PlateKind kind, int row)
    {
        static_assert(static_cast<int>(OutlineMode::ByNature) == kOutlineModeCount - 1);
        return CanOutlineByNature(kind, row) ? kOutlineModeCount : kOutlineModeCount - 1;
    }

    int Stepped(int value, int step) { return static_cast<int>(std::lround(static_cast<double>(value) / step)) * step; }
}
