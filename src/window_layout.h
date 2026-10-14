#pragma once

#include "generated/window_pieces.h"
#include "plate_settings.h"

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace headsup
{
    // The settings window's pages; the layout gives their order, names and keys.
    enum class Page : int
    {
        Entities,
        Mobs,
        Players,
        Self,
        Npcs,
        Pets,
        UserInterface,
        TopPanel,
        Widgets,
        MouseCursor,
        ConfigMenu,
        Debug,
    };
    constexpr int kPageCount = static_cast<int>(Page::Debug) + 1;

    // A kind as a bit of a kinds mask: which of the entity pages a card, a setting or a row is drawn on.
    constexpr uint8_t KindBit(PlateKind kind) { return static_cast<uint8_t>(1u << static_cast<int>(kind)); }
    constexpr uint8_t kEveryKind = (1u << kPlateKindCount) - 1;

    // The settings window as data/settings_window.json lays it out (tools/gen_window.py writes the tables): its
    // pages, the cards on each, the settings in each card, and their words. A text by kind is an array in
    // PlateKind's order, null where one text serves every kind. first and count are places in the layout's arrays.
    struct LayoutOption
    {
        const char* name;
        const char* tip;
        const char* otherTip; // the second tip an option has where the code has one: the claim rows' Inherit
    };
    struct LayoutRow
    {
        uint16_t row; // the piece's own row enum
        const char* key;
        const char* name;
        const char* tip;
        const char* const* nameByKind;
        const char* const* tipByKind;
        const char* otherTip; // the bottom line's Padding
        uint8_t kinds;
    };
    struct LayoutSetting
    {
        SettingPiece piece;
        const char* key;
        const char* name;
        const char* tip;
        const char* const* nameByKind;
        const char* const* tipByKind;
        uint8_t kinds;
        bool entity; // part of an entity's design: drawn for its page's kind
        int firstOption, optionCount, firstRow, rowCount;
    };
    struct LayoutCard
    {
        CardPiece piece;
        const char* key;
        const char* name;
        const char* about;
        uint8_t kinds;
        int firstSetting, settingCount;
    };
    struct LayoutPage
    {
        Page page;
        const char* key;
        const char* name;
        bool child; // indented under the page before it
        int firstCard, cardCount; // the five entity pages share one range
    };
    struct LayoutList
    {
        WindowList list;
        const char* key;
        int firstOption, optionCount;
    };
    struct LayoutWord
    {
        WindowWord word;
        const char* name;
        const char* tip;
    };

    struct WindowLayout
    {
        std::span<const LayoutPage> pages; // in the sidebar's order
        std::span<const LayoutCard> cards;
        std::span<const LayoutSetting> settings;
        std::span<const LayoutRow> rows;
        std::span<const LayoutOption> options;
        std::span<const LayoutList> lists;
        std::span<const LayoutWord> words;

        const LayoutPage& PageOf(Page page) const;
        std::optional<Page> PageNamed(std::string_view key) const;
        std::span<const LayoutCard> CardsOf(const LayoutPage& page) const;
        std::span<const LayoutSetting> SettingsOf(const LayoutCard& card) const;
        std::span<const LayoutRow> RowsOf(const LayoutSetting& setting) const;
        std::span<const LayoutOption> OptionsOf(const LayoutSetting& setting) const;
        std::span<const LayoutOption> OptionsOf(WindowList list) const;
        const LayoutSetting* SettingOf(SettingPiece piece) const; // none where a test's layout left it out
        const LayoutWord& WordOf(WindowWord word) const;
    };
    extern const WindowLayout kWindowLayout;

    // A word of the layout as text: a tip the file leaves out is none.
    inline std::string_view Said(const char* text) { return text != nullptr ? std::string_view(text) : std::string_view(); }

    // An entry's words on a kind's page: the kind's own where it has words by kind, the shared ones otherwise.
    const char* NameOf(const LayoutSetting& setting, std::optional<PlateKind> kind);
    const char* TipOf(const LayoutSetting& setting, std::optional<PlateKind> kind);
    const char* NameOf(const LayoutRow& row, std::optional<PlateKind> kind);
    const char* TipOf(const LayoutRow& row, std::optional<PlateKind> kind);
}
