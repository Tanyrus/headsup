#include "plate_settings.h"

#include <algorithm>

namespace headsup
{
    namespace
    {
        const char* const kMobRowKeys[kCategoryCount]   = {"aggressive", "passive", "noData", "aggressiveNm", "passiveNm", "placeholder", "partyClaim", "othersClaim", "dead"};
        const char* const kMobRowLabels[kCategoryCount] = {"Aggressive", "Passive", "No Data", "NM (Aggressive)", "NM (Passive)", "NM (Placeholder)", "Claimed (Yours)", "Claimed (Others')", "Dead"};
        const char* const kPlayerRowKeys[kPlayerRowCount]   = {"player", "partyMember", "friend", "linkshell"};
        const char* const kPlayerRowLabels[kPlayerRowCount] = {"Players", "Party Members", "Friends", "Linkshell Members"};
        const char* const kPetRowKeys[kPetRowCount]   = {"yourPet", "partyPet", "friendPet", "othersPet"};
        const char* const kPetRowLabels[kPetRowCount] = {"Your Pets", "Party Pets", "Friend Pets", "Others' Pets"};
        const char* const kOneRowKeys[kPlateKindCount]   = {"", "", "self", "npc", ""};
        const char* const kOneRowLabels[kPlateKindCount] = {"", "", "Self", "NPCs", ""};
        const char* const kKindKeys[kPlateKindCount]     = {"mobs", "players", "self", "npcs", "pets"};
        const char* const kConditionLabels[kShowConditionCount] = {"When Targeted", "When Unclaimed", "When Claimed by You",
            "When Claimed by Others", "When Engaged in Combat", "When Too Weak", "When Aggressive", "When Passive",
            "When Party Member", "When Friend", "When Linkshell Member", "When Owner is You"};
        const char* const kSourceLabels[kColorSourceCount] = {"Custom", "Vanilla Color", "Color by Aggression", "Color by Toughness",
            "Color by Claim Status", "Color by Targeting", "Color by Entity Type"};
        const char* const kDetectionLabels[kDetectionIconCount] = {"Aggression", "Links", "Sight", "True Sight", "Sound",
            "Scent", "Use of Spells", "Use of Abilities", "Low Player HP"};
        const char* const kPlateModeLabels[kPlateModeCount]     = {"Hide Nameplate", "Show Nameplate", "When Targeted", "Vanilla Nameplate"};
        const char* const kOutlineModeLabels[kOutlineModeCount] = {"No Outline", "Show Outline", "When Targeted", "Inherit"};
        const char* const kNatureLabels[kNatureCount] = {"Passive", "Aggressive", "NM (Passive)", "NM (Aggressive)", "NM (Placeholder)", "No Data"};
        const char* const kToughnessLabels[kLabelShadeCount] = {"Unknown Level", "Too Weak", "Easy Prey", "Decent Challenge",
            "Even Match", "Tough", "Very Tough"};
        const char* const kClaimLabels[kClaimStateCount]        = {"Unclaimed", "Claimed (Others')", "Claimed (Yours)"};
        const char* const kTargetStateLabels[kTargetStateCount] = {"Not Targeted", "Targeted", "Locked On", "Sub-Target", "Out of Range"};
        const char* const kTargetStateTips[kTargetStateCount]   = {nullptr, nullptr, nullptr, "Picking for a spell, ability or trade.",
            "A sub-target out of the spell or ability's range."};
        const char* const kTargetTypeLabels[kTargetTypeCount]   = {"Mob", "Player", "Friend", "Party Member", "Linkshell Member",
            "NPC or Object", "Pet"};
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
        mobs.lines[1].textSize     = 87; // 13 px of 15
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

    const char* RowLabel(PlateKind kind, int row)
    {
        if (kind == PlateKind::Mobs) return kMobRowLabels[row];
        if (kind == PlateKind::Players) return kPlayerRowLabels[row];
        if (kind == PlateKind::Pets) return kPetRowLabels[row];
        return kOneRowLabels[static_cast<int>(kind)];
    }

    const char* KindKey(PlateKind kind) { return kKindKeys[static_cast<int>(kind)]; }

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
        case PlateKind::Pets: return {ShowCondition::Targeted, ShowCondition::OwnerIsYou};
        default: return {ShowCondition::Targeted};
        }
    }

    const char* ConditionLabel(ShowCondition c) { return kConditionLabels[static_cast<int>(c)]; }

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


    const char* const kAlignLabels[kAlignCount]                 = {"Left", "Center", "Right"};
    const char* const kFontStyleLabels[kFontStyleCount]         = {"Bold", "Italics", "Underline"};
    const char* const kOrnamentPlaceLabels[kOrnamentPlaceCount] = {"Disabled", "Top", "Above Middle Line", "Below Middle Line",
        "Bottom"};
    const char* const kLineLabels[kPlateLines]                  = {"Top Line", "Middle Line", "Bottom Line"};

    int OutlineModesFor(PlateKind kind, int row)
    {
        static_assert(static_cast<int>(OutlineMode::ByNature) == kOutlineModeCount - 1);
        return CanOutlineByNature(kind, row) ? kOutlineModeCount : kOutlineModeCount - 1;
    }
}
