#include "window_layout.h"
#include "layout_edit.h"
#include "test.h"

#include <algorithm>
#include <string>

using namespace headsup;

TEST(every_page_is_in_the_layout_once_and_found_by_its_key)
{
    CHECK_EQ(kWindowLayout.pages.size(), static_cast<size_t>(kPageCount));
    for (int p = 0; p < kPageCount; ++p)
    {
        const auto page = static_cast<Page>(p);
        CHECK(kWindowLayout.PageOf(page).page == page);
        CHECK(kWindowLayout.PageNamed(kWindowLayout.PageOf(page).key) == page);
    }
    CHECK(!kWindowLayout.PageNamed(""));
    CHECK(!kWindowLayout.PageNamed(kWindowLayout.PageOf(Page::TopPanel).name)); // by its key, not its name
}

TEST(the_entity_pages_share_one_range_of_cards)
{
    const LayoutPage& mobs = kWindowLayout.PageOf(Page::Mobs);
    CHECK(mobs.cardCount > 0);
    for (const Page page : {Page::Players, Page::Self, Page::Npcs, Page::Pets})
    {
        const LayoutPage& other = kWindowLayout.PageOf(page);
        CHECK(other.firstCard == mobs.firstCard && other.cardCount == mobs.cardCount);
    }
    CHECK(kWindowLayout.PageOf(Page::Entities).firstCard != mobs.firstCard);
}

TEST(every_setting_the_code_draws_is_in_one_card_of_the_layout)
{
    for (int piece = 0; piece < kSettingPieceCount; ++piece)
    {
        int found = 0;
        for (const LayoutCard& card : kWindowLayout.cards)
            for (const LayoutSetting& setting : kWindowLayout.SettingsOf(card))
                found += setting.piece == static_cast<SettingPiece>(piece) ? 1 : 0;
        CHECK_EQ(found, 1);
        CHECK(kWindowLayout.SettingOf(static_cast<SettingPiece>(piece)) != nullptr);
    }
}

TEST(a_text_by_kind_is_the_kinds_own_and_the_shared_text_otherwise)
{
    const LayoutSetting* styles = kWindowLayout.SettingOf(SettingPiece::LineStyles);
    CHECK(styles != nullptr);
    const auto rows = kWindowLayout.RowsOf(*styles);
    const auto glow = std::ranges::find(rows, static_cast<uint16_t>(LineStylesRow::DisableGlowInCombat), &LayoutRow::row);
    const auto font = std::ranges::find(rows, static_cast<uint16_t>(LineStylesRow::Font), &LayoutRow::row);
    CHECK(glow != rows.end() && font != rows.end());
    const std::string mobs = TipOf(*glow, PlateKind::Mobs), players = TipOf(*glow, PlateKind::Players);
    CHECK(!mobs.empty() && !players.empty() && mobs != players);
    CHECK(TipOf(*font, PlateKind::Mobs) == font->tip && TipOf(*font, PlateKind::Pets) == font->tip);
}

TEST(a_list_and_a_word_are_found_by_their_piece)
{
    CHECK_EQ(kWindowLayout.OptionsOf(WindowList::PlateMode).size(), static_cast<size_t>(kPlateModeCount));
    CHECK_EQ(kWindowLayout.OptionsOf(WindowList::PlateLines).size(), static_cast<size_t>(kPlateLines));
    CHECK(kWindowLayout.WordOf(WindowWord::Always).word == WindowWord::Always);
    CHECK(kWindowLayout.WordOf(WindowWord::LineStylesDoNotSync).word == WindowWord::LineStylesDoNotSync);
}

TEST(a_setting_moved_in_an_edited_layout_is_in_the_other_cards_range)
{
    test::EditedLayout edited;
    edited.MoveSetting("chocobo", "settings-window", 0);
    const WindowLayout layout = edited.Layout();
    const auto cardOf = [&](const char* key) {
        return std::ranges::find_if(layout.cards, [&](const LayoutCard& c) { return std::string(c.key) == key; });
    };
    const auto settings = layout.SettingsOf(*cardOf("settings-window"));
    CHECK(std::string(settings.front().key) == "chocobo");
    const auto pointer = layout.SettingsOf(*cardOf("mouse-pointer"));
    CHECK(std::ranges::none_of(pointer, [](const LayoutSetting& s) { return std::string(s.key) == "chocobo"; }));
    CHECK_EQ(layout.settings.size(), kWindowLayout.settings.size());
}
