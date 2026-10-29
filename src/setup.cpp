#include "setup.h"

#include "cursor_picture.h"

#include <algorithm>
#include <iterator>

namespace headsup
{
    namespace
    {
        KindSettings& KindOf(Settings& s, PlateKind kind) { return s.Kind(kind); }
        const KindSettings& DefaultKind(PlateKind kind)
        {
            static const std::array<KindSettings, kPlateKindCount> kDefaults = DefaultKinds();
            return kDefaults[static_cast<size_t>(kind)];
        }
        void EveryRow(Settings& s, PlateKind kind, PlateMode mode)
        {
            KindSettings& k = KindOf(s, kind);
            for (int row = 0; row < RowCountOf(kind); ++row)
                k.rows[row].plate = mode;
        }

        template <BarTheme T>
        void Theme(Settings& s)
        {
            s.barTheme = T;
        }
        const SetupOption kThemes[]{{"phoenix-gold", Theme<BarTheme::PhoenixGold>}, {"umbra-dark", Theme<BarTheme::UmbraDark>},
            {"phoenix-red-ornate", Theme<BarTheme::PhoenixRedOrnate>}, {"phoenix-red-pills", Theme<BarTheme::PhoenixRedPills>},
            {"aurora", Theme<BarTheme::Aurora>}, {"game-window", Theme<BarTheme::GameWindow>}};
        const SetupChoice kThemeChoices[]{{"theme", kThemes, 0}};
        const SettingPiece kThemeSettings[]{SettingPiece::WindowType};

        const SettingPiece kFontSettings[]{SettingPiece::DefaultFont, SettingPiece::BaseFontSize, SettingPiece::BaseIconSize,
            SettingPiece::TextFont, SettingPiece::TextSize, SettingPiece::Size};

        // The mob rows of what would attack you, notorious monsters and their placeholders, and claimed mobs.
        constexpr Category kAttackRows[]{Category::WillAttack, Category::NmWillAttack, Category::NmWontAttack, Category::Placeholder,
            Category::PartyClaim, Category::OtherClaim};
        void AttackersShown(Settings& s)
        {
            EveryRow(s, PlateKind::Mobs, PlateMode::WhenTargeted);
            for (Category row : kAttackRows)
                KindOf(s, PlateKind::Mobs).rows[CategoryIndex(row)].plate = PlateMode::Shown;
        }
        const SetupOption kMobPlates[]{{"all", [](Settings& s) { EveryRow(s, PlateKind::Mobs, PlateMode::Shown); }},
            {"attackers", AttackersShown}, {"target", [](Settings& s) { EveryRow(s, PlateKind::Mobs, PlateMode::WhenTargeted); }},
            {"game", [](Settings& s) { EveryRow(s, PlateKind::Mobs, PlateMode::Vanilla); }}};

        // A design of the default mob lines, the top ones of which show nothing.
        void MobLines(Settings& s, int emptyLines, OrnamentPlace ornament)
        {
            KindSettings& mobs = KindOf(s, PlateKind::Mobs);
            const KindSettings& defaults = DefaultKind(PlateKind::Mobs);
            for (int line = 0; line < kPlateLines; ++line)
            {
                mobs.lines[line] = defaults.lines[line];
                if (line < emptyLines) mobs.lines[line].text.clear();
            }
            mobs.ornament       = defaults.ornament;
            mobs.ornament.place = ornament;
        }
        const SetupOption kMobDesigns[]{{"full", [](Settings& s) { MobLines(s, 0, DefaultKind(PlateKind::Mobs).ornament.place); }},
            {"simple", [](Settings& s) { MobLines(s, 1, OrnamentPlace::Disabled); }},
            {"name", [](Settings& s) { MobLines(s, 2, OrnamentPlace::Disabled); }}};
        const SetupChoice kMobChoices[]{{"mob-plates", kMobPlates, 0}, {"mob-design", kMobDesigns, 0}};

        // Outlines on some mob rows: the claim rows follow the row under them, the dead have none.
        void MobOutlines(Settings& s, std::span<const Category> rows, OutlineMode mode)
        {
            s.outlines         = true;
            KindSettings& mobs = KindOf(s, PlateKind::Mobs);
            for (int row = 0; row < kCategoryCount; ++row)
                mobs.rows[row].outline = IsClaimRow(static_cast<Category>(row)) ? OutlineMode::ByNature : OutlineMode::Disabled;
            for (Category row : rows)
                mobs.rows[CategoryIndex(row)].outline = mode;
        }
        constexpr Category kOutlinedByDefault[]{Category::WillAttack, Category::NmWillAttack, Category::NmWontAttack, Category::Placeholder};
        constexpr Category kLivingRows[]{Category::WillAttack, Category::WontAttack, Category::Unknown, Category::NmWillAttack,
            Category::NmWontAttack, Category::Placeholder};
        const SetupOption kOutlines[]{{"attackers", [](Settings& s) { MobOutlines(s, kOutlinedByDefault, OutlineMode::Shown); }},
            {"all", [](Settings& s) { MobOutlines(s, kLivingRows, OutlineMode::Shown); }},
            {"target", [](Settings& s) { MobOutlines(s, kLivingRows, OutlineMode::WhenTargeted); }},
            // The rows are kept, for outlines turned back on in the settings window to show as they did.
            {"none", [](Settings& s) { s.outlines = false; }}};
        template <int Percent>
        void Thickness(Settings& s)
        {
            s.outlineThickness = Percent;
        }
        const SetupOption kThicknesses[]{{"thin", Thickness<50>}, {"normal", Thickness<100>}, {"thick", Thickness<200>}};
        const SetupChoice kOutlineChoices[]{{"outlines", kOutlines, 0}, {"outline-thickness", kThicknesses, 1}};

        template <CursorStyle Style>
        void Cursor(Settings& s)
        {
            if (Style != CursorStyle::Vanilla)
                s.cursorWidth = std::clamp(NaturalCursorWidth(Style, s.cursorHeight), kMinCursorSide, kMaxCursorSide);
            s.cursorStyle = Style;
            s.cursorFile.clear();
        }
        const SetupOption kCursors[]{{"feather", Cursor<CursorStyle::Feather>}, {"arrow", Cursor<CursorStyle::Triangle>},
            {"chocobo", Cursor<CursorStyle::Chocobo>}, {"game", Cursor<CursorStyle::Vanilla>}};
        const SetupChoice kCursorChoices[]{{"cursor", kCursors, 0}};
        const SettingPiece kCursorSettings[]{SettingPiece::Color};

        template <PlateKind Kind, PlateMode Mode>
        void Plates(Settings& s)
        {
            EveryRow(s, Kind, Mode);
        }
        const SetupOption kPlayers[]{{"headsup", Plates<PlateKind::Players, PlateMode::Shown>}, {"game", Plates<PlateKind::Players, PlateMode::Vanilla>}};
        const SetupOption kSelf[]{{"shown", Plates<PlateKind::Self, PlateMode::Shown>}, {"hidden", Plates<PlateKind::Self, PlateMode::Hidden>},
            {"game", Plates<PlateKind::Self, PlateMode::Vanilla>}};
        const SetupOption kNpcs[]{{"headsup", Plates<PlateKind::Npcs, PlateMode::Shown>}, {"game", Plates<PlateKind::Npcs, PlateMode::Vanilla>}};
        const SetupOption kPets[]{{"targeted", Plates<PlateKind::Pets, PlateMode::WhenTargeted>}, {"shown", Plates<PlateKind::Pets, PlateMode::Shown>},
            {"game", Plates<PlateKind::Pets, PlateMode::Vanilla>}};
        const SetupChoice kOtherChoices[]{{"players", kPlayers, 0}, {"self", kSelf, 0}, {"npcs", kNpcs, 0}, {"pets", kPets, 0}};

        const SetupOption kPanel[]{{"shown", [](Settings& s) { s.showBar = true; }}, {"hidden", [](Settings& s) { s.showBar = false; }}};
        const SetupChoice kWidgetChoices[]{{"panel", kPanel, 0}};

        template <PointerIcon Icon>
        void Pointer(Settings& s)
        {
            s.pointerIcon = Icon;
            s.pointerFile.clear();
        }
        const SetupOption kPointers[]{{"chocobo", Pointer<PointerIcon::Chocobo>}, {"game", Pointer<PointerIcon::Vanilla>}};
        const SetupChoice kMouseChoices[]{{"pointer", kPointers, 1}};
        const SettingPiece kExtraSettings[]{SettingPiece::AutoCheck, SettingPiece::LookUpPlayers, SettingPiece::StackNames,
            SettingPiece::HideDuringConversation};

        const SetupPage kPages[]{
            {SetupStep::Welcome, "welcome", {}, {}},
            {SetupStep::Theme, "theme", kThemeChoices, kThemeSettings},
            {SetupStep::Fonts, "fonts", {}, kFontSettings},
            {SetupStep::MobNameplates, "mob-nameplates", kMobChoices, {}},
            {SetupStep::Outlines, "outlines", kOutlineChoices, {}},
            {SetupStep::TargetCursor, "target-cursor", kCursorChoices, kCursorSettings},
            {SetupStep::Others, "others", kOtherChoices, {}},
            {SetupStep::Widgets, "widgets", kWidgetChoices, {}},
            {SetupStep::MouseExtras, "mouse-extras", kMouseChoices, kExtraSettings},
        };
        static_assert(std::size(kPages) == kSetupSteps);

        constexpr BarWidget kWhere[]{BarWidget::Zone, BarWidget::Weather, BarWidget::Distance, BarWidget::World, BarWidget::Conquest};
        constexpr BarWidget kTime[]{BarWidget::VanaTime, BarWidget::EarthTime};
        constexpr BarWidget kYou[]{BarWidget::Job, BarWidget::Exp, BarWidget::Gil, BarWidget::Bags, BarWidget::Party, BarWidget::Target};
        constexpr BarWidget kSystem[]{BarWidget::Fps, BarWidget::Connection, BarWidget::Memory, BarWidget::GameHelp, BarWidget::Signet};
        const SetupWidgetGroup kWidgetGroups[]{{"where", kWhere}, {"time", kTime}, {"you", kYou}, {"system", kSystem}};
    }

    std::span<const SetupPage> SetupPages() { return kPages; }
    const SetupPage& SetupPageOf(SetupStep step) { return kPages[static_cast<size_t>(step)]; }

    std::optional<size_t> PickedOption(const SetupChoice& choice, const Settings& s)
    {
        for (size_t i = 0; i < choice.options.size(); ++i)
        {
            Settings picked = s;
            choice.options[i].apply(picked);
            if (picked == s) return i;
        }
        return std::nullopt;
    }

    std::span<const SetupWidgetGroup> SetupWidgetGroups() { return kWidgetGroups; }

    void SetWidgetShown(Settings& s, BarWidget widget, bool shown)
    {
        if (shown)
        {
            s.Widget(PanelWidget{widget}).on = true;
            return;
        }
        s.Widget(PanelWidget{widget}).on = false;
        for (int copy = 1; copy < kMostCopies; ++copy)
            RemoveWidgetCopy(s, PanelWidget{widget, copy});
    }

    void UseSetupDefaults(Settings& s)
    {
        for (const SetupPage& page : kPages)
            for (const SetupChoice& choice : page.choices)
                choice.options[choice.defaultOption].apply(s);
        const Settings fresh;
        s.windowType    = fresh.windowType;
        s.defaultFont   = fresh.defaultFont;
        s.baseSize      = fresh.baseSize;
        s.baseIconSize  = fresh.baseIconSize;
        s.barTextFont   = fresh.barTextFont;
        s.barTextSize   = fresh.barTextSize;
        s.menuFontSize  = fresh.menuFontSize;
        s.cursorColor   = fresh.cursorColor;
        s.autoCheck     = fresh.autoCheck;
        s.lookUpPlayers = fresh.lookUpPlayers;
        s.stackNames    = fresh.stackNames;
        s.hideInEvent   = fresh.hideInEvent;
        for (int w = 0; w < kBarWidgetCount; ++w)
            SetWidgetShown(s, static_cast<BarWidget>(w), Shown(fresh, static_cast<BarWidget>(w)));
    }
}
