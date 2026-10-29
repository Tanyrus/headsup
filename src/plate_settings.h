#pragma once

#include "color.h"
#include "generated/mobflags.h"
#include "labels.h"

#include <span>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace headsup
{
    enum class Category : uint8_t
    {
        WillAttack,
        WontAttack,
        Unknown,
        NmWillAttack,
        NmWontAttack,
        Placeholder, // a lottery placeholder nobody has claimed, whatever it would do
        PartyClaim,  // claimed by you or your party, whatever else it is
        OtherClaim,  // and claimed by anyone else
        Dead,        // dead, whatever it was and whoever claimed it
    };
    constexpr int kCategoryCount = static_cast<int>(Category::Dead) + 1;
    constexpr int CategoryIndex(Category category) { return static_cast<int>(category); }
    // The menu's order for the mob rows: ordinary mobs, notorious monsters and their placeholders, claimed mobs, dead
    // ones, then mobs with no data.
    constexpr Category kMobRowOrder[kCategoryCount] = {Category::WillAttack, Category::WontAttack, Category::NmWillAttack,
        Category::NmWontAttack, Category::Placeholder, Category::PartyClaim, Category::OtherClaim, Category::Dead,
        Category::Unknown};

    enum class PlateKind : uint8_t { Mobs, Players, Self, Npcs, Pets };
    constexpr int kPlateKindCount = 5;
    enum class PlateMode : uint8_t { Hidden, Shown, WhenTargeted, Vanilla };
    constexpr int kPlateModeCount = 4;
    // ByNature, on the Placeholder and claim rows of mobs only, steps aside for the row under it.
    enum class OutlineMode : uint8_t { Disabled, Shown, WhenTargeted, ByNature };
    constexpr int kOutlineModeCount = 4;
    constexpr bool IsClaimRow(Category row) { return row == Category::PartyClaim || row == Category::OtherClaim; }
    constexpr bool CanOutlineByNature(PlateKind kind, int row)
    {
        const Category category = static_cast<Category>(row);
        return kind == PlateKind::Mobs && (category == Category::Placeholder || IsClaimRow(category));
    }
    enum class ColorSource : uint8_t { Custom, Game, Aggression, Toughness, Claim, Target, Type };
    constexpr int kColorSourceCount = 7;
    struct ColorPick
    {
        ColorSource source = ColorSource::Custom;
        Color custom       = ColorFromRgb(0xFFFFFF);
        bool operator==(const ColorPick&) const = default;
    };
    enum class Nature : uint8_t { Passive, Aggressive, PassiveNm, AggressiveNm, Placeholder, NoData };
    constexpr int kNatureCount = 6;
    // CalledForHelp: its claimer called for help, so anyone may fight it (the server drops the claim then).
    enum class ClaimState : uint8_t { Unclaimed, Others, Yours, CalledForHelp };
    constexpr int kClaimStateCount = 4;
    enum class TargetState : uint8_t { None, Targeted, Locked, SubTarget, OutOfRange };
    constexpr int kTargetStateCount = 5;
    enum class TargetType : uint8_t { Mob, Player, Friend, PartyMember, LinkshellMember, Npc, Pet }; // the menu's order
    constexpr int kTargetTypeCount = 7;
    enum class PlayerRow : uint8_t { Player, PartyMember, Friend, LinkshellMember };
    constexpr int kPlayerRowCount = 4;
    enum class PetRow : uint8_t { Yours, Party, Friend, Others }; // by its owner
    constexpr int kPetRowCount = 4;
    constexpr int kMaxRows        = kCategoryCount; // the mob rows are Category

    struct Palette
    {
        Color aggression[kNatureCount];
        Color toughness[kLabelShadeCount];
        Color claim[kClaimStateCount];
        Color target[kTargetStateCount];
        Color type[kTargetTypeCount];
        bool operator==(const Palette&) const = default;
    };
    Palette DefaultPalette();

    enum class ShowCondition : uint8_t
    {
        Targeted, Unclaimed, YourClaim, OthersClaim, Engaged, TooWeak, Aggressive, Passive, PartyMember, Friend, LinkshellMember,
        OwnerIsYou, Generally
    };
    constexpr int kShowConditionCount = static_cast<int>(ShowCondition::Generally) + 1;
    constexpr uint32_t ConditionBit(ShowCondition c) { return 1u << static_cast<int>(c); }
    // What Generally holds without. Every mob is unclaimed or claimed and aggressive or passive, and your fight is
    // true of every one at once, so those are not.
    constexpr bool IsSpecialState(ShowCondition c)
    {
        return c != ShowCondition::Unclaimed && c != ShowCondition::Aggressive && c != ShowCondition::Passive &&
               c != ShowCondition::Engaged && c != ShowCondition::Generally;
    }
    struct LineShow
    {
        bool always   = true;
        uint32_t when = 0;
        bool operator==(const LineShow&) const = default;
    };
    // A line's font styles: any of them together, as bits.
    enum class FontStyle : uint8_t { Bold = 1, Italic = 2, Underline = 4 };
    constexpr int kFontStyleCount    = 3;
    constexpr uint8_t kAllFontStyles = 7;
    constexpr uint8_t StyleBit(FontStyle style) { return static_cast<uint8_t>(style); }
    constexpr bool HasStyle(uint8_t styles, FontStyle style) { return (styles & StyleBit(style)) != 0; }
    enum class Align : uint8_t { Left, Center, Right };
    constexpr int kAlignCount = 3;
    constexpr int kPlateLines = 3;
    constexpr int kNoSync     = -1;
    constexpr int kMinSizePercent = 25, kMaxSizePercent = 400; // text, icon and timer sizes, the scale limits
    constexpr int kMinGlow = 0, kMaxGlow = 300;                // 0 is no glow
    constexpr int kMinGlowSize = 25, kMaxGlowSize = 200;
    constexpr int kMinShadow = 0, kMaxShadow = 200;
    constexpr float kShadowUnit = 1.5f; // a shadow strength of 100 percent, in the mockup's shadows
    constexpr int kMinLinePadding = -5, kMaxLinePadding = 10; // pixels
    constexpr int kMinLineOffset = -50, kMaxLineOffset = 50;  // pixels
    constexpr int kMinOrnamentWidth = 20, kMaxOrnamentWidth = 300;       // pixels
    constexpr int kMinOrnamentThickness = 3, kMaxOrnamentThickness = 24; // pixels
    constexpr size_t kMaxTemplate = 255;                                 // bytes of a line's text
    // A player's status icons, less GM's, which is drawn whatever the limit: mentor, new adventurer, level sync,
    // away, linkshell, bazaar and seeking party.
    constexpr int kCountedIcons = 7;
    // Whether a kind's nameplates can show those icons, so that its page sets their limit and its file keeps it.
    constexpr bool HasStatusIcons(PlateKind kind) { return kind == PlateKind::Players || kind == PlateKind::Self; }

    // The tags a template is built from.
    constexpr const char* kTagDetect       = "$detect";
    constexpr const char* kTagWeakResist   = "$weak$resist";
    constexpr const char* kTagStatus       = "$status";
    constexpr const char* kTagImmune       = "$immune";
    constexpr const char* kTagLevelCon     = "Lv $level $con";
    constexpr const char* kTagId           = "$id";
    constexpr const char* kTagServerId     = "$serverid";
    constexpr const char* kTagPlaceholder  = "$ph";
    constexpr const char* kTagName         = "$name";
    constexpr const char* kTagGm           = "$gm";
    constexpr const char* kTagMentor       = "$mentor";
    constexpr const char* kTagNew          = "$new";
    constexpr const char* kTagSync         = "$sync";
    constexpr const char* kTagAway         = "$away";
    constexpr const char* kTagLinkshell    = "$linkshell";
    constexpr const char* kTagBazaar       = "$bazaar";
    constexpr const char* kTagSeeking      = "$seeking";
    // The default players' name line, which the migration of an all-default file must give back.
    constexpr const char* kTagPlayerName   = "$mentor$new$sync$away$linkshell$bazaar$seeking$gm$name";

    struct LineDesign
    {
        std::string text;
        int syncWith = kNoSync;
        LineShow show;
        Align align = Align::Center;
        std::string font;
        uint8_t style = 0; // FontStyle bits
        int textSize  = 100;
        int iconSize = 100;
        int padding  = 0; // pixels of extra space under the line; over it for the bottom line, which stays on the name
        int offset   = 0; // pixels right of where its alignment puts it, left below 0; it moves this line alone
        ColorPick color;
        int shadow = 100;
        ColorPick shadowColor{ColorSource::Custom, ColorFromRgb(0x000000)};
        int glow     = 100;
        int glowSize = 100;
        ColorPick glowColor{ColorSource::Aggression};
        bool glowOffInCombat = false; // no glow while you fight: on for a mob's lines
        bool operator==(const LineDesign&) const = default;
    };

    constexpr int kTopLine = 0, kMiddleLine = 1, kBottomLine = kPlateLines - 1; // the design's lines
    // Where a kind's diamond rule goes: over the plate's first line, between the middle line and the one over or
    // under it while both show, or under the plate's last line.
    enum class OrnamentPlace : uint8_t { Disabled, Top, AboveMiddle, BelowMiddle, Bottom };
    constexpr int kOrnamentPlaceCount = 5;
    struct OrnamentSettings
    {
        OrnamentPlace place = OrnamentPlace::Disabled;
        int width          = 90;
        int height         = 6;
        ColorPick color{ColorSource::Aggression};
        bool operator==(const OrnamentSettings&) const = default;
    };
    struct RowSettings
    {
        PlateMode plate     = PlateMode::Shown;
        OutlineMode outline = OutlineMode::Disabled;
        ColorPick outlineColor{ColorSource::Aggression};
        bool operator==(const RowSettings&) const = default;
    };
    // How opaque a kind's nameplates are, in percent, by what you target: with no target, as another than your
    // target, and as your target. Fading in, each is whole within kFadeNearYalms and almost nothing at the range.
    constexpr float kPercent = 100.0f; // what a setting in percent is a share of
    constexpr int kPercentStep = 5;    // what every setting in percent moves by
    int Stepped(int value, int step);  // the multiple of step nearest value
    constexpr float kMinOutlineDistance = 15.0f, kMaxOutlineDistance = 50.0f; // yalms: the game draws no mob past 50

    struct OpacitySettings
    {
        int noTarget    = 100;
        int notTargeted = 100;
        int targeted    = 100;
        bool fadeIn     = false;
        float range     = kMaxOutlineDistance;
        bool operator==(const OpacitySettings&) const = default;
    };
    struct KindSettings
    {
        RowSettings rows[kMaxRows];
        LineDesign lines[kPlateLines];
        OrnamentSettings ornament;
        int maxIcons = kCountedIcons; // the status icons a nameplate draws at most, GM's aside
        bool operator==(const KindSettings&) const = default;
    };
    std::array<KindSettings, kPlateKindCount> DefaultKinds();
    // Two lines of the design trade places, text and style, and a line that syncs with either still syncs with it.
    void SwapLines(KindSettings& kind, int a, int b);

    int RowCountOf(PlateKind kind);                // 9, 4, 1, 1, 4
    const char* RowKey(PlateKind kind, int row);   // "aggressive", "partyMember", "self", ...: settings keys
    const char* KindKey(PlateKind kind);           // "mobs", "players", "self", "npcs", "pets"
    // The conditions a kind's lines may be shown by, in the menu's order.
    std::span<const ShowCondition> ConditionsFor(PlateKind kind);
    // The color sources a kind's picks offer, in the menu's order: text is true for a text color, which may take the
    // game's own, last.
    std::vector<ColorSource> SourcesFor(PlateKind kind, bool text);
    // What the target cursor's color and its edge's may follow: every palette, since it sits on anything.
    std::vector<ColorSource> CursorSources();

    enum class HookAction : uint8_t { Keep, Hide, Replace }; // the game draws the name; nothing draws it; HeadsUp draws
    enum class DetectionIcon : uint8_t { Aggro, Link, Sight, TrueSight, Sound, Scent, Magic, Ability, Blood };
    constexpr int kDetectionIconCount = 9;
    constexpr uint32_t kAllDetection  = (1u << kDetectionIconCount) - 1;
    // What a new install shows: all but the aggression icon, which the outline's color already tells.
    constexpr uint32_t kDefaultDetection = kAllDetection & ~(1u << static_cast<int>(DetectionIcon::Aggro));
    constexpr uint32_t DetectionBit(DetectionIcon d) { return 1u << static_cast<int>(d); }

    // How many of the outline modes a row's dropdown offers, from the first: all but ByNature where it cannot be.
    int OutlineModesFor(PlateKind kind, int row);
    // Whether a row in this mode draws an outline in its own color pick: under ByNature another row's pick is used.
    constexpr bool HasOutlineColor(OutlineMode mode)
    {
        return mode == OutlineMode::Shown || mode == OutlineMode::WhenTargeted;
    }
    // Whether HeadsUp draws a plate, always or at times, for a row in this mode.
    constexpr bool DrawsPlate(PlateMode mode) { return mode != PlateMode::Hidden && mode != PlateMode::Vanilla; }
}
