#include "window_layout.h"

#include "bar_placement.h"
#include "bar_types.h"
#include "dash_mark.h"
#include "labels.h"
#include "menu_document.h"
#include "outline_math.h"
#include "plate_template.h"
#include "settings.h"

#include <algorithm>

namespace headsup
{
    namespace
    {
#include "generated/window_layout.inc"

        const char* ByKind(const char* shared, const char* const* byKind, std::optional<PlateKind> kind)
        {
            return kind && byKind != nullptr && byKind[static_cast<int>(*kind)] != nullptr ? byKind[static_cast<int>(*kind)] : shared;
        }

        constexpr int ListCount(WindowList list)
        {
            for (const LayoutList& listed : kLayoutLists)
                if (listed.list == list) return listed.optionCount;
            return -1;
        }
        constexpr int OptionCount(SettingPiece piece)
        {
            for (const LayoutSetting& setting : kLayoutSettings)
                if (setting.piece == piece) return setting.optionCount;
            return -1;
        }
        constexpr int RowCount(SettingPiece piece)
        {
            for (const LayoutSetting& setting : kLayoutSettings)
                if (setting.piece == piece) return setting.rowCount;
            return -1;
        }
    }

    // tools/gen_window.py holds the file to its own counts; these hold its counts to the code's.
    static_assert(std::size(kLayoutPages) == kPageCount, "settings_window.json: a page for each Page");
    static_assert(ListCount(WindowList::PlateMode) == kPlateModeCount, "settings_window.json: plate-mode");
    static_assert(ListCount(WindowList::OutlineMode) == kOutlineModeCount, "settings_window.json: outline-mode");
    static_assert(ListCount(WindowList::ColorSource) == kColorSourceCount, "settings_window.json: color-source");
    static_assert(ListCount(WindowList::ShowCondition) == kShowConditionCount, "settings_window.json: show-condition");
    static_assert(ListCount(WindowList::OrnamentPlace) == kOrnamentPlaceCount, "settings_window.json: ornament-place");
    static_assert(ListCount(WindowList::Align) == kAlignCount, "settings_window.json: align");
    static_assert(ListCount(WindowList::FontStyle) == kFontStyleCount, "settings_window.json: font-style");
    static_assert(ListCount(WindowList::Smoothness) == kSmoothnessLevelCount, "settings_window.json: smoothness");
    static_assert(ListCount(WindowList::BarTheme) == kBarThemeCount, "settings_window.json: bar-theme");
    static_assert(ListCount(WindowList::MenuTheme) == kMenuThemeCount, "settings_window.json: menu-theme");
    static_assert(ListCount(WindowList::BarSections) == kBarSectionCount, "settings_window.json: bar-sections");
    static_assert(ListCount(WindowList::BarWidgets) == kBarWidgetCount, "settings_window.json: bar-widgets");
    static_assert(ListCount(WindowList::PlateLines) == kPlateLines, "settings_window.json: plate-lines");
    static_assert(ListCount(WindowList::JobLayout) == kJobLayoutCount, "settings_window.json: job-layout");
    static_assert(ListCount(WindowList::ExpBar) == kExpBarPlaceCount, "settings_window.json: exp-bar");
    static_assert(ListCount(WindowList::Tags) == kTagCount, "settings_window.json: tags");
    static_assert(OptionCount(SettingPiece::ReplaceDashes) == kDashMarkCount, "settings_window.json: replace-dashes");
    static_assert(OptionCount(SettingPiece::CursorStyle) == kCursorStyleCount, "settings_window.json: cursor-style");
    static_assert(OptionCount(SettingPiece::Separators) == kBarSeparatorsCount, "settings_window.json: separators");
    static_assert(OptionCount(SettingPiece::OneLine) == kSubtitlePlaceCount, "settings_window.json: one-line");
    static_assert(RowCount(SettingPiece::AggressionColors) == kNatureCount, "settings_window.json: aggression-colors");
    static_assert(RowCount(SettingPiece::ToughnessColors) == kLabelShadeCount, "settings_window.json: toughness-colors");
    static_assert(static_cast<int>(ToughnessColorsRow::IncrediblyTough) == static_cast<int>(LabelShade::IncrediblyTough),
        "gen_window.py: toughness-colors rows in LabelShade's order");
    static_assert(RowCount(SettingPiece::ClaimColors) == kClaimStateCount, "settings_window.json: claim-colors");
    static_assert(RowCount(SettingPiece::TargetingColors) == kTargetStateCount, "settings_window.json: targeting-colors");
    static_assert(RowCount(SettingPiece::EntityTypeColors) == kTargetTypeCount, "settings_window.json: entity-type-colors");
    static_assert(RowCount(SettingPiece::Detection) == kDetectionIconCount, "settings_window.json: detection");
    static_assert(RowCount(SettingPiece::StatusTable) == kStatusRankCount, "settings_window.json: status-table");

    const WindowLayout kWindowLayout{kLayoutPages, kLayoutCards, kLayoutSettings, kLayoutRows, kLayoutOptions,
        kLayoutLists, kLayoutWords};

    const LayoutPage& WindowLayout::PageOf(Page page) const
    {
        return *std::ranges::find(pages, page, &LayoutPage::page);
    }

    std::optional<Page> WindowLayout::PageNamed(std::string_view key) const
    {
        const auto at = std::ranges::find(pages, key, [](const LayoutPage& p) { return std::string_view(p.key); });
        return at != pages.end() ? std::optional(at->page) : std::nullopt;
    }

    std::span<const LayoutCard> WindowLayout::CardsOf(const LayoutPage& page) const
    {
        return cards.subspan(static_cast<size_t>(page.firstCard), static_cast<size_t>(page.cardCount));
    }

    std::span<const LayoutSetting> WindowLayout::SettingsOf(const LayoutCard& card) const
    {
        return settings.subspan(static_cast<size_t>(card.firstSetting), static_cast<size_t>(card.settingCount));
    }

    std::span<const LayoutRow> WindowLayout::RowsOf(const LayoutSetting& setting) const
    {
        return rows.subspan(static_cast<size_t>(setting.firstRow), static_cast<size_t>(setting.rowCount));
    }

    std::span<const LayoutOption> WindowLayout::OptionsOf(const LayoutSetting& setting) const
    {
        return options.subspan(static_cast<size_t>(setting.firstOption), static_cast<size_t>(setting.optionCount));
    }

    std::span<const LayoutOption> WindowLayout::OptionsOf(WindowList list) const
    {
        const LayoutList& listed = *std::ranges::find(lists, list, &LayoutList::list);
        return options.subspan(static_cast<size_t>(listed.firstOption), static_cast<size_t>(listed.optionCount));
    }

    const LayoutSetting* WindowLayout::SettingOf(SettingPiece piece) const
    {
        const auto at = std::ranges::find(settings, piece, &LayoutSetting::piece);
        return at != settings.end() ? &*at : nullptr;
    }

    const LayoutWord& WindowLayout::WordOf(WindowWord word) const { return *std::ranges::find(words, word, &LayoutWord::word); }

    std::string Filled(std::string_view text, std::string_view mark, std::string_view with)
    {
        std::string out;
        for (size_t at = 0;;)
        {
            const size_t found = text.find(mark, at);
            out += text.substr(at, found - at);
            if (found == std::string_view::npos) return out;
            out += with;
            at = found + mark.size();
        }
    }

    const char* NameOf(const LayoutSetting& setting, std::optional<PlateKind> kind) { return ByKind(setting.name, setting.nameByKind, kind); }
    const char* TipOf(const LayoutSetting& setting, std::optional<PlateKind> kind) { return ByKind(setting.tip, setting.tipByKind, kind); }
    const char* NameOf(const LayoutRow& row, std::optional<PlateKind> kind) { return ByKind(row.name, row.nameByKind, kind); }
    const char* TipOf(const LayoutRow& row, std::optional<PlateKind> kind) { return ByKind(row.tip, row.tipByKind, kind); }
}
