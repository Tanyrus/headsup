#include "bar_game_help.h"
#include "test.h"

#include <string>
#include <vector>

using namespace headsup;

TEST(a_long_help_line_breaks_at_words_and_counts_characters_not_bytes)
{
    CHECK(WrapWords("Select a target to auto-attack.", 80) == (std::vector<std::string>{"Select a target to auto-attack."}));
    CHECK(WrapWords("one two three four", 9) == (std::vector<std::string>{"one two", "three", "four"}));
    CHECK(WrapWords("abcdefghijkl", 5) == (std::vector<std::string>{"abcdefghijkl"})); // a word longer than a line
    // Three full-width characters, nine bytes, are three characters.
    CHECK(WrapWords("\xEF\xBC\x9F\xEF\xBC\x9F\xEF\xBC\x9F x", 5).size() == 1);
    CHECK(WrapWords("", 80).empty());
}

namespace
{
    // Every character 10 px wide: a room of 100 px holds 10.
    float TenEach(const std::string& text) { return 10.0f * static_cast<float>(text.size()); }
    constexpr float kGap = 20.0f;
}

TEST(the_help_bar_shows_the_caption_then_help_that_fits_untouched)
{
    CHECK(HelpBarLine(StripText{"Commands", "Select a target."}, 400.0f, kGap, TenEach, TenEach) ==
          (HelpBarText{"Commands", "Select a target.", false}));
    CHECK(HelpBarLine(StripText{"", "Saving..."}, 400.0f, kGap, TenEach, TenEach) ==
          (HelpBarText{"", "Saving...", false})); // no caption: the help alone
    CHECK(HelpBarLine(StripText{"Commands", ""}, 400.0f, kGap, TenEach, TenEach) ==
          (HelpBarText{"Commands", "", false})); // or the caption alone
    // Spaces the game pads with or doubles are not shown.
    CHECK(HelpBarLine(StripText{"", "  one  two  "}, 400.0f, kGap, TenEach, TenEach).help == "one two");
}

TEST(help_too_long_for_what_the_caption_leaves_ends_in_dots_and_says_it_was_cut)
{
    // 300 px less the caption's 80 and the gap's 20 leaves 200: 20 characters, the dots among them.
    const HelpBarText bar =
        HelpBarLine(StripText{"Commands", "Select a target to auto-attack."}, 300.0f, kGap, TenEach, TenEach);
    CHECK(bar.caption == "Commands" && bar.help == "Select a target t..." && bar.cut);
    // Without a caption the help has the whole room and no gap to leave.
    CHECK(HelpBarLine(StripText{"", "Select a target to auto-attack."}, 300.0f, kGap, TenEach, TenEach).help ==
          "Select a target to auto-att...");
}

TEST(a_caption_wider_than_the_room_is_cut_and_leaves_the_help_none)
{
    const HelpBarText bar = HelpBarLine(StripText{"Temporary Items", "ok"}, 100.0f, kGap, TenEach, TenEach);
    CHECK(bar.caption.ends_with("...") && TenEach(bar.caption) <= 100.0f);
    CHECK(bar.help.empty() && bar.cut);
}

TEST(the_help_bars_box_wraps_its_caption_the_bags_count_and_its_help_in_one_row)
{
    // From x 8: padding 16, the caption's 80, a gap of 6, the count's 40, padding again, the help's 200, padding.
    const HelpBarRow row = PlaceHelpBar(8.0f, 50.0f, 28.0f, 16.0f, 6.0f, 80.0f, 40.0f, 200.0f);
    CHECK(row.captionX == 24.0f && row.bagX == 110.0f && row.helpX == 166.0f);
    CHECK(row.box == (BarRect{8.0f, 50.0f, 374.0f, 28.0f}));
    // The help alone starts at the padding, and the box ends a padding past it.
    const HelpBarRow help = PlaceHelpBar(8.0f, 50.0f, 28.0f, 16.0f, 6.0f, 0.0f, 0.0f, 200.0f);
    CHECK(help.helpX == 24.0f && help.box.width == 232.0f);
    // So does the caption alone.
    CHECK(PlaceHelpBar(8.0f, 50.0f, 28.0f, 16.0f, 6.0f, 80.0f, 0.0f, 0.0f).box.width == 112.0f);
}
