#include "plate_settings.h"

#include <algorithm>

namespace headsup
{
    namespace
    {
        const char* const kMobRowKeys[kCategoryCount]   = {"aggressive", "passive", "noData", "aggressiveNm", "passiveNm", "placeholder", "partyClaim", "othersClaim"};
        const char* const kMobRowLabels[kCategoryCount] = {"Aggressive", "Passive", "No Data", "Aggressive NM", "Passive NM", "NM Placeholder", "Party Claim", "Others' Claim"};
        const char* const kPlayerRowKeys[kPlayerRowCount]   = {"player", "partyMember", "friend", "linkshell"};
        const char* const kPlayerRowLabels[kPlayerRowCount] = {"Players", "Party Members", "Friends", "Linkshell Co-Members"};
        const char* const kOneRowKeys[kPlateKindCount]   = {"", "", "self", "npc", "pet"};
        const char* const kOneRowLabels[kPlateKindCount] = {"", "", "Self", "NPCs", "Pets"};
        const char* const kKindKeys[kPlateKindCount]     = {"mobs", "players", "self", "npcs", "pets"};
        const char* const kKindLabels[kPlateKindCount]   = {"Mobs", "Players", "Self", "NPCs", "Pets"};
        const char* const kConditionLabels[kShowConditionCount] = {"When Targeted", "Unclaimed", "Your Claim", "Others' Claim",
            "Engaged in Combat", "Too Weak", "Aggressive", "Passive", "Party Member", "Friend", "Linkshell Co-Member"};
        const char* const kSourceLabels[kColorSourceCount] = {"Custom", "Game's", "Aggression", "Toughness", "Claim", "Target Status", "Target Type"};
        const char* const kDetectionLabels[kDetectionIconCount] = {"Aggressive or Passive", "Links", "Sight", "True Sight", "Sound",
            "Scent", "Magic", "Job Abilities", "Low HP"};
        const char* const kPlateModeLabels[kPlateModeCount]     = {"Hidden", "Shown", "When Targeted", "Expanding", "Vanilla"};
        const char* const kOutlineModeLabels[kOutlineModeCount] = {"Disabled", "Shown", "When Targeted", "By Nature"};
        const char* const kNatureLabels[kNatureCount] = {"Passive", "Aggressive", "Passive NM", "Aggressive NM", "NM Placeholder", "No Data"};
        const char* const kToughnessLabels[kLabelShadeCount] = {"Unknown Level", "Too Weak", "Easy Prey", "Decent Challenge",
            "Even Match", "Tough", "Very Tough"};
        const char* const kClaimLabels[kClaimStateCount]        = {"Unclaimed", "Others' Claim", "Your Claim"};
        const char* const kTargetStateLabels[kTargetStateCount] = {"Not Targeted", "Targeted", "Locked On", "Sub-Target", "Out of Range"};
        const char* const kTargetStateTips[kTargetStateCount]   = {nullptr, nullptr, nullptr, "Picking for a spell, ability or trade.",
            "A sub-target out of the spell or ability's range."};
        const char* const kTargetTypeLabels[kTargetTypeCount]   = {"Mob", "NPC or Object", "Pet", "Player", "Friend", "Party Member",
            "Linkshell Co-Member"};
        const char* const kStatusRankLabels[kStatusRankCount]   = {"Paralyze", "Bind", "Silence", "Slow", "Poison", "Light Sleep",
            "Dark Sleep", "Blind", "Stun", "Gravity"};

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
            {{0.45f, 0.70f, 1.00f}}, {{1.00f, 1.00f, 1.00f}}, {{1.00f, 1.00f, 0.35f}}, {{1.00f, 0.35f, 0.35f}}};
        const Color claim[kClaimStateCount] = {{{1.00f, 0.95f, 0.60f}}, {{0.55f, 0.45f, 0.40f}}, {{0.30f, 0.65f, 1.00f}}};
        const Color target[kTargetStateCount] = {{{1.00f, 1.00f, 1.00f}}, ColorFromRgb(kPhoenixRed), {{0.65f, 0.40f, 1.00f}},
            {{0.99f, 0.82f, 0.09f}}, {{1.00f, 0.25f, 0.25f}}};
        const Color type[kTargetTypeCount] = {{{1.00f, 1.00f, 1.00f}}, {{1.00f, 1.00f, 1.00f}}, {{0.70f, 0.95f, 0.80f}},
            {{1.00f, 1.00f, 1.00f}}, {{1.00f, 0.90f, 0.55f}}, {{0.55f, 0.80f, 1.00f}}, {{0.70f, 1.00f, 0.70f}}};
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
        mobs.lines[0].iconSize     = 107; // 16 px of a 15 px base, as before
        mobs.lines[1].text         = kTagLevelCon;
        mobs.lines[1].textSize     = 87; // 13 px of 15
        mobs.lines[1].color.source = ColorSource::Toughness;
        mobs.lines[2]              = NameLine(kTagName);
        mobs.lines[2].glow         = 100;
        mobs.ornament.on           = true;
        const LineDesign player = NameLine(kTagPlayerName);
        kinds[static_cast<int>(PlateKind::Players)].lines[2] = player;
        kinds[static_cast<int>(PlateKind::Self)].lines[2]    = player;
        kinds[static_cast<int>(PlateKind::Npcs)].lines[2]    = NameLine(kTagName);
        kinds[static_cast<int>(PlateKind::Pets)].lines[2]    = NameLine(kTagName);
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
        default: return 1;
        }
    }

    const char* RowKey(PlateKind kind, int row)
    {
        if (kind == PlateKind::Mobs) return kMobRowKeys[row];
        if (kind == PlateKind::Players) return kPlayerRowKeys[row];
        return kOneRowKeys[static_cast<int>(kind)];
    }

    const char* RowLabel(PlateKind kind, int row)
    {
        if (kind == PlateKind::Mobs) return kMobRowLabels[row];
        if (kind == PlateKind::Players) return kPlayerRowLabels[row];
        return kOneRowLabels[static_cast<int>(kind)];
    }

    const char* KindKey(PlateKind kind) { return kKindKeys[static_cast<int>(kind)]; }
    const char* KindLabel(PlateKind kind) { return kKindLabels[static_cast<int>(kind)]; }

    std::vector<ShowCondition> ConditionsFor(PlateKind kind)
    {
        switch (kind)
        {
        case PlateKind::Mobs:
            return {ShowCondition::Targeted, ShowCondition::Unclaimed, ShowCondition::YourClaim, ShowCondition::OthersClaim,
                ShowCondition::Engaged, ShowCondition::TooWeak, ShowCondition::Aggressive, ShowCondition::Passive};
        case PlateKind::Players:
            return {ShowCondition::Targeted, ShowCondition::PartyMember, ShowCondition::Friend, ShowCondition::LinkshellMember};
        case PlateKind::Self: return {ShowCondition::Targeted, ShowCondition::Engaged};
        default: return {ShowCondition::Targeted};
        }
    }

    const char* ConditionLabel(ShowCondition c) { return kConditionLabels[static_cast<int>(c)]; }

    std::vector<ColorSource> SourcesFor(PlateKind kind, bool text)
    {
        std::vector<ColorSource> sources{ColorSource::Custom};
        if (text) sources.push_back(ColorSource::Game);
        if (kind == PlateKind::Mobs)
            sources.insert(sources.end(), {ColorSource::Aggression, ColorSource::Toughness, ColorSource::Claim});
        sources.insert(sources.end(), {ColorSource::Target, ColorSource::Type});
        return sources;
    }

    const char* SourceLabel(ColorSource s) { return kSourceLabels[static_cast<int>(s)]; }
    const char* DetectionLabel(DetectionIcon d) { return kDetectionLabels[static_cast<int>(d)]; }
    const char* PlateModeLabel(PlateMode mode) { return kPlateModeLabels[static_cast<int>(mode)]; }
    const char* OutlineModeLabel(OutlineMode mode) { return kOutlineModeLabels[static_cast<int>(mode)]; }
    const char* NatureLabel(Nature nature) { return kNatureLabels[static_cast<int>(nature)]; }
    const char* ToughnessLabel(LabelShade shade) { return kToughnessLabels[static_cast<int>(shade)]; }
    const char* ClaimLabel(ClaimState claim) { return kClaimLabels[static_cast<int>(claim)]; }
    const char* TargetStateLabel(TargetState target) { return kTargetStateLabels[static_cast<int>(target)]; }
    const char* TargetTypeLabel(TargetType type) { return kTargetTypeLabels[static_cast<int>(type)]; }
    const char* StatusRankLabel(StatusRank status) { return kStatusRankLabels[static_cast<int>(status)]; }
    const char* TargetStateTip(TargetState target) { return kTargetStateTips[static_cast<int>(target)]; }

    bool IsCharacterName(std::string_view name)
    {
        const auto letter = [](char c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'); };
        return name.size() >= kMinCharacterName && name.size() <= kMaxCharacterName && std::ranges::all_of(name, letter);
    }

    const char* const kAlignLabels[kAlignCount]                 = {"Left", "Center", "Right"};
    const char* const kOrnamentPlaceLabels[kOrnamentPlaceCount] = {"Lines 1-2", "Lines 2-3"};
    const char* const kLineLabels[kPlateLines]                  = {"Line 1", "Line 2", "Line 3"};

    int OutlineModesFor(PlateKind kind, int row)
    {
        static_assert(static_cast<int>(OutlineMode::ByNature) == kOutlineModeCount - 1);
        return CanOutlineByNature(kind, row) ? kOutlineModeCount : kOutlineModeCount - 1;
    }
}
