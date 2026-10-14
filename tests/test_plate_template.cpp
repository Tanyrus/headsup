#include "plate_template.h"
#include "test.h"

#include <algorithm>
#include <utility>

using namespace headsup;

namespace
{
    Atom Text(const char* t) { return Atom{{}, t}; }
    Atom Of(TagSlot slot) { return Atom{std::move(slot), ""}; }
    Atom Of(Tag tag) { return Of(TagSlot{tag}); }
}

TEST(text_and_tags_parse_in_order)
{
    const std::vector<Atom> atoms = ParseTemplate("Lv $level $con");
    CHECK_EQ(atoms.size(), size_t{4});
    CHECK(atoms[0] == Text("Lv "));
    CHECK(atoms[1] == Of(Tag::Level));
    CHECK(atoms[2] == Text(" "));
    CHECK(atoms[3] == Of(Tag::Con));
}

TEST(adjacent_tags_need_no_separator_and_a_tag_ends_at_the_first_non_letter)
{
    const std::vector<Atom> atoms = ParseTemplate("$seeking$bazaar $name($level)");
    CHECK_EQ(atoms.size(), size_t{7});
    CHECK(atoms[0] == Of(Tag::Seeking));
    CHECK(atoms[1] == Of(Tag::Bazaar));
    CHECK(atoms[2] == Text(" "));
    CHECK(atoms[3] == Of(Tag::Name));
    CHECK(atoms[4] == Text("("));
    CHECK(atoms[5] == Of(Tag::Level));
    CHECK(atoms[6] == Text(")"));
}

TEST(unknown_tags_and_lone_dollars_stay_text)
{
    CHECK(ParseTemplate("$nothing here") == std::vector<Atom>{Text("$nothing here")});
    CHECK(ParseTemplate("costs 5$") == std::vector<Atom>{Text("costs 5$")});
    CHECK(ParseTemplate("$ name") == std::vector<Atom>{Text("$ name")});
    CHECK(ParseTemplate("$$name") == std::vector<Atom>{Text("$name")});
    CHECK(ParseTemplate("$NAME") == std::vector<Atom>{Text("$NAME")});
    CHECK(ParseTemplate("") == std::vector<Atom>{});
}

TEST(every_tag_has_a_name_that_parses_back_to_it)
{
    for (int t = 0; t < kTagCount; ++t)
    {
        const auto tag = static_cast<Tag>(t);
        CHECK(ParseTemplate(TagName(tag)) == std::vector<Atom>{Of(tag)});
        CHECK(TagTip(tag)[0] != '\0');
    }
}

TEST(each_kind_offers_its_own_tags)
{
    const std::vector<Tag> mobs = TagsFor(PlateKind::Mobs);
    CHECK(std::ranges::find(mobs, Tag::Detect) != mobs.end());
    CHECK(std::ranges::find(mobs, Tag::Seeking) == mobs.end());
    const std::vector<Tag> players = TagsFor(PlateKind::Players);
    CHECK(std::ranges::find(players, Tag::Linkshell) != players.end());
    CHECK(std::ranges::find(players, Tag::Con) == players.end());
    CHECK(TagsFor(PlateKind::Self) == players);
    CHECK(TagsFor(PlateKind::Npcs) == std::vector<Tag>{Tag::Name});
    CHECK((TagsFor(PlateKind::Pets) == std::vector<Tag>{Tag::Name, Tag::Owner}));
}

TEST(a_design_is_its_stretches_of_text_as_typed_and_the_tags_between_them)
{
    // One stretch more than tags: before the first, between each two and after the last, empty or not.
    CHECK((RunsOf("Lv $level $con") == TemplateRuns{{"Lv ", " ", ""}, {{Tag::Level}, {Tag::Con}}}));
    CHECK((RunsOf("$name") == TemplateRuns{{"", ""}, {{Tag::Name}}}));
    CHECK((RunsOf("$id$serverid") == TemplateRuns{{"", "", ""}, {{Tag::Id}, {Tag::ServerId}}}));
    CHECK((RunsOf("") == TemplateRuns{{""}, {}}));
    // What is no tag stays as it was typed, its marks too.
    CHECK((RunsOf("$$5 $nope $namex") == TemplateRuns{{"$$5 $nope $namex"}, {}}));
    // Joined again it is the same text.
    for (const char* text : {"Lv $level $con", "$name", "", "$$5 $nope $namex", "$id$serverid", "a $name (b) $job/$level",
             "\xC3\x97$name\xC3\x97"})
        CHECK_EQ(JoinRuns(RunsOf(text)), text);
}

TEST(a_tag_a_letter_follows_is_written_in_braces_so_that_its_name_ends)
{
    // "$names" would be no tag at all: the name is set in braces where a small letter comes straight after it.
    const TemplateRuns plural{{"", "s"}, {{Tag::Name}}};
    CHECK_EQ(JoinRuns(plural), "${name}s");
    CHECK(RunsOf("${name}s") == plural);
    CHECK((ParseTemplate("${name}s") == std::vector<Atom>{Of(Tag::Name), Text("s")}));
    CHECK_EQ(JoinRuns(TemplateRuns{{"", "S", " x"}, {{Tag::Name}, {Tag::Job}}}), "$nameS$job x"); // only a small letter would join it
    // Braces around no tag, or left open, are text.
    CHECK((ParseTemplate("${nope} ${name") == std::vector<Atom>{Text("${nope} ${name")}));
    CHECK((RunsOf("${nope}") == TemplateRuns{{"${nope}"}, {}}));
    // A stretch that ends in a lone mark would swallow the tag after it: the mark is doubled.
    CHECK_EQ(JoinRuns(TemplateRuns{{"a$", ""}, {{Tag::Name}}}), "a$$$name");
    CHECK((ParseTemplate("a$$$name") == std::vector<Atom>{Text("a$"), Of(Tag::Name)}));
    CHECK_EQ(JoinRuns(TemplateRuns{{"a$$", ""}, {{Tag::Name}}}), "a$$$name"); // a doubled one already is one mark
}

TEST(a_tag_goes_in_at_the_caret_which_then_stands_after_it)
{
    std::string text = "Lv  EP";
    RunCaret caret{0, 3};
    CHECK(PutTag(text, caret, {Tag::Level}));
    CHECK_EQ(text, "Lv $level EP");
    CHECK((caret == RunCaret{1, 0}));
    CHECK(PutTag(text, caret, {Tag::Con})); // the next chip clicked lands after the first
    CHECK_EQ(text, "Lv $level$con EP");
    CHECK((caret == RunCaret{2, 0}));
    // Between two letters, the tag's name is closed off from the one after it.
    std::string word = "ab";
    RunCaret middle{0, 1};
    CHECK(PutTag(word, middle, {Tag::Name}));
    CHECK_EQ(word, "a${name}b");
    // A caret never set, or left past a text that has since shortened, is the end.
    RunCaret unset;
    CHECK(PutTag(text, unset, {Tag::Ph}));
    CHECK_EQ(text, "Lv $level$con EP$ph");
    CHECK((unset == RunCaret{3, 0}));
    RunCaret past{9, 99};
    CHECK(PutTag(text, past, {Tag::Id}));
    CHECK_EQ(text, "Lv $level$con EP$ph$id");
    // Half a tag is a design nobody wrote: one that will not fit is refused, and the caret stays.
    std::string full(kMaxTemplate - 2, 'x');
    RunCaret at{0, 0};
    CHECK(!PutTag(full, at, {Tag::Name}));
    CHECK_EQ(full, std::string(kMaxTemplate - 2, 'x'));
    CHECK((at == RunCaret{0, 0}));
    std::string room(kMaxTemplate - 5, 'X');
    CHECK(PutTag(room, at, {Tag::Name}) && room.size() == kMaxTemplate);
}

TEST(a_tag_taken_out_joins_the_text_either_side_of_it_and_a_stretch_can_be_typed_anew)
{
    RunCaret caret;
    CHECK_EQ(WithoutTag("Lv $level end $con", 0, kLastTag, caret), "Lv  end $con");
    CHECK((caret == RunCaret{0, 3})); // where the two stretches met
    CHECK_EQ(WithoutTag("Lv $level end $con", 1, kLastTag, caret), "Lv $level end ");
    CHECK((caret == RunCaret{1, 5}));
    CHECK_EQ(WithoutTag("$name", 0, kLastTag, caret), "");
    CHECK_EQ(WithoutTag("$name", 4, kLastTag, caret), "$name"); // no such tag: nothing changes
    // A stretch typed anew may hold tags of its own.
    CHECK_EQ(WithRun("Lv $level", 0, "Level "), "Level $level");
    CHECK_EQ(WithRun("Lv $level", 1, " $con"), "Lv $level $con");
    CHECK_EQ(WithRun("Lv $level", 1, "s"), "Lv ${level}s");
    CHECK_EQ(WithRun("Lv $level", 7, "x"), "Lv $level"); // no such stretch
    // What would pass the longest a design may be is cut to fit.
    CHECK_EQ(WithRun("$name", 1, std::string(kMaxTemplate, 'y')).size(), kMaxTemplate);
}

TEST(tags_in_one_pair_of_braces_share_a_slot)
{
    CHECK((ParseTemplate("${sync|bazaar} $name") ==
           std::vector<Atom>{Of(TagSlot{Tag::Sync, Tag::Bazaar}), Text(" "), Of(Tag::Name)}));
    const TemplateRuns shared{{"", " x"}, {{Tag::Sync, Tag::Bazaar, Tag::Seeking}}};
    CHECK(RunsOf("${sync|bazaar|seeking} x") == shared);
    CHECK_EQ(JoinRuns(shared), "${sync|bazaar|seeking} x");
    // Without the braces a bar is a bar somebody typed, between two tags of their own.
    CHECK((ParseTemplate("$sync|$bazaar") == std::vector<Atom>{Of(Tag::Sync), Text("|"), Of(Tag::Bazaar)}));
    CHECK_EQ(JoinRuns(RunsOf("$sync|$bazaar")), "$sync|$bazaar");
    // One name that is no tag, a name left out or a brace left open, and none of it is a slot.
    for (const char* text : {"${sync|nope}", "${sync|}", "${|sync}", "${sync||bazaar}", "${sync|bazaar"})
    {
        CHECK((RunsOf(text) == TemplateRuns{{text}, {}}));
        CHECK_EQ(JoinRuns(RunsOf(text)), text);
    }
    KindSettings kind;
    kind.lines[0].text = "${title|linkshell}";
    CHECK(UsesTag(kind, Tag::Title) && UsesTag(kind, Tag::Linkshell) && !UsesTag(kind, Tag::Bazaar));
}

TEST(a_tag_joins_a_slot_once_and_only_while_the_line_has_room)
{
    std::string text = "$sync $name";
    CHECK(JoinSlot(text, 0, {Tag::Bazaar}));
    CHECK_EQ(text, "${sync|bazaar} $name");
    CHECK(JoinSlot(text, 0, {Tag::Seeking, Tag::Away})); // a slot dropped on another brings all of its tags
    CHECK_EQ(text, "${sync|bazaar|seeking|away} $name");
    CHECK(!JoinSlot(text, 0, {Tag::Bazaar, Tag::Sync})); // it holds both already
    CHECK(JoinSlot(text, 0, {Tag::Bazaar, Tag::Gm}));    // only the one it lacks goes in
    CHECK_EQ(text, "${sync|bazaar|seeking|away|gm} $name");
    CHECK(JoinSlot(text, 1, {Tag::Title}));
    CHECK_EQ(text, "${sync|bazaar|seeking|away|gm} ${name|title}");
    CHECK(!JoinSlot(text, 2, {Tag::Gm})); // no such slot
    CHECK(!JoinSlot(text, -1, {Tag::Gm}));
    // "$sync" grows by the braces and "|bazaar", nine bytes: with eight to spare it is refused and nothing changes.
    const std::string full = std::string(kMaxTemplate - 5 - 8, 'x') + "$sync";
    std::string line       = full;
    CHECK(!JoinSlot(line, 0, {Tag::Bazaar}));
    CHECK_EQ(line, full);
    line.erase(0, 1);
    CHECK(JoinSlot(line, 0, {Tag::Bazaar}) && line.size() == kMaxTemplate);
}

TEST(a_slot_loses_one_tag_at_a_time_and_goes_with_its_last)
{
    // Backspace beside a chip that holds several tags takes the last, and the caret stays where it was.
    RunCaret caret{1, 0};
    CHECK_EQ(WithoutTag("a ${sync|bazaar|away} b", 0, kLastTag, caret), "a ${sync|bazaar} b");
    CHECK((caret == RunCaret{1, 0}));
    CHECK_EQ(WithoutTag("a ${sync|bazaar} b", 0, kLastTag, caret), "a $sync b");
    CHECK((caret == RunCaret{1, 0}));
    CHECK_EQ(WithoutTag("${sync|bazaar}s", 0, kLastTag, caret), "${sync}s"); // the one left still ends before the letter
    // Dragged out, one tag goes alone, whichever of them it is.
    CHECK_EQ(WithoutTag("a ${sync|bazaar|away} b", 0, 0, caret), "a ${bazaar|away} b");
    CHECK_EQ(WithoutTag("a ${sync|bazaar|away} b", 0, 1, caret), "a ${sync|away} b");
    CHECK((caret == RunCaret{1, 0}));
    // The last one a slot holds takes the slot with it: the stretches either side join, and the caret is where they met.
    CHECK_EQ(WithoutTag("a $sync b", 0, kLastTag, caret), "a  b");
    CHECK((caret == RunCaret{0, 2}));
    CHECK_EQ(WithoutTag("a $sync b$name", 1, 0, caret), "a $sync b");
    // No such slot, or no such tag in it: nothing changes.
    CHECK_EQ(WithoutTag("$name", 3, 0, caret), "$name");
    CHECK_EQ(WithoutTag("$name", -1, kLastTag, caret), "$name");
    CHECK_EQ(WithoutTag("${sync|bazaar}", 0, 2, caret), "${sync|bazaar}");
}

TEST(a_tag_of_a_slot_takes_the_place_of_another_and_with_it_its_turn)
{
    // The first of a slot's tags that has something to show is drawn, so their order is their priority.
    std::string text = "x ${sync|bazaar|away}";
    CHECK(MoveTag(text, 0, 0, 2));
    CHECK_EQ(text, "x ${bazaar|away|sync}");
    CHECK(MoveTag(text, 0, 2, 0));
    CHECK_EQ(text, "x ${sync|bazaar|away}");
    CHECK(MoveTag(text, 0, 1, 0));
    CHECK_EQ(text, "x ${bazaar|sync|away}");
    // Onto itself, from or to a place the slot does not have, or in a slot the line does not have: nothing.
    CHECK(!MoveTag(text, 0, 1, 1));
    CHECK(!MoveTag(text, 0, 3, 0));
    CHECK(!MoveTag(text, 0, 0, 3));
    CHECK(!MoveTag(text, 1, 0, 1));
    CHECK_EQ(text, "x ${bazaar|sync|away}");
}

TEST(a_design_uses_a_tag_when_any_line_holds_it)
{
    KindSettings kind;
    kind.lines[1].text = "Lv $level";
    CHECK(UsesTag(kind, Tag::Level));
    CHECK(!UsesTag(kind, Tag::Con));
}

TEST(a_card_of_tags_no_line_holds_says_which_tag_would_show_it)
{
    KindSettings kind;
    kind.lines[0].text = "$detect";
    kind.lines[2].text = "$name";
    CHECK_EQ(UnusedTagsNote(kind, {Tag::Detect}, kShowsTheseEnding), "");
    CHECK_EQ(UnusedTagsNote(kind, {Tag::Status}, kShowsTheseEnding), "Put the Statuses tag on a line of the design to show these on a nameplate.");
    CHECK_EQ(UnusedTagsNote(kind, {Tag::Weak, Tag::Resist}, kShowsTheseEnding),
        "Put the Weaknesses or Resistances tag on a line of the design to show these on a nameplate.");
    CHECK_EQ(UnusedTagsNote(kind, {Tag::Weak, Tag::Resist, Tag::Status, Tag::Immune}, kShowsTheseEnding),
        "Put the Weaknesses, Resistances, Statuses or Immunities tag on a line of the design to show these on a nameplate.");
    kind.lines[1].text = "Lv $level $resist";
    CHECK_EQ(UnusedTagsNote(kind, {Tag::Weak, Tag::Resist}, kShowsTheseEnding), ""); // one of a card's tags is enough
    CHECK_EQ(UnusedTagsNote(kind, {Tag::Weak, Tag::Resist, Tag::Status, Tag::Immune}, kShowsTheseEnding), "");
    CHECK_EQ(UnusedTagsNote(kind, {Tag::Weak}, kShowsTheseEnding), "Put the Weaknesses tag on a line of the design to show these on a nameplate.");
    kind.lines[1].text = "$$resist"; // a doubled mark is a dollar sign, not the tag
    CHECK(!UnusedTagsNote(kind, {Tag::Resist}, kShowsTheseEnding).empty());
}

TEST(a_design_without_the_name_tag_says_why_no_name_shows)
{
    KindSettings kind;
    kind.lines[1].text = "Lv $level";
    CHECK_EQ(UnusedTagsNote(kind, {Tag::Name}, kShowsNameEnding),
        "Put the Name tag on a line of the design to show a name: the game's own is hidden where HeadsUp draws the nameplate.");
    kind.lines[0].text = "<$name>";
    CHECK_EQ(UnusedTagsNote(kind, {Tag::Name}, kShowsNameEnding), "");
    CHECK(!UnusedTagsNote(KindSettings{}, {Tag::Name}, kShowsNameEnding).empty()); // three empty lines
}

TEST(the_default_templates_use_only_tags_the_catalog_knows)
{
    for (const char* text : {kTagDetect, kTagWeakResist, kTagStatus, kTagImmune, kTagLevelCon, kTagId, kTagServerId,
             kTagPlaceholder, kTagName, kTagPlayerName})
        for (const Atom& atom : ParseTemplate(text))
            CHECK(!atom.slot.empty() || atom.text.find(kTagMark) == std::string::npos);
    CHECK((ParseTemplate(kTagLevelCon) == std::vector<Atom>{Text("Lv "), Of(Tag::Level), Text(" "), Of(Tag::Con)}));
    CHECK((ParseTemplate(kTagPlayerName) == std::vector<Atom>{Of(Tag::Mentor), Of(Tag::New), Of(Tag::Sync), Of(Tag::Away),
                                               Of(Tag::Linkshell), Of(Tag::Bazaar), Of(Tag::Seeking), Of(Tag::Gm), Of(Tag::Name)}));
}

TEST(every_tag_is_offered_and_each_kind_lists_its_chips_in_order)
{
    for (int t = 0; t < kTagCount; ++t)
    {
        bool offered = false;
        for (int k = 0; k < kPlateKindCount; ++k)
        {
            const std::vector<Tag> tags = TagsFor(static_cast<PlateKind>(k));
            offered = offered || std::ranges::find(tags, static_cast<Tag>(t)) != tags.end();
        }
        CHECK(offered);
    }
    CHECK((TagsFor(PlateKind::Mobs) == std::vector<Tag>{Tag::Name, Tag::Level, Tag::Con, Tag::Id, Tag::ServerId, Tag::Ph,
                                           Tag::Detect, Tag::Weak, Tag::Resist, Tag::Status, Tag::Immune}));
    CHECK((TagsFor(PlateKind::Players) == std::vector<Tag>{Tag::Name, Tag::Gm, Tag::Mentor, Tag::New, Tag::Sync, Tag::Away,
                                              Tag::Linkshell, Tag::Bazaar, Tag::Seeking, Tag::Job, Tag::Level, Tag::Title}));
}

TEST(a_tag_name_runs_to_the_last_letter_and_text_around_unknown_tags_stays_whole)
{
    CHECK(ParseTemplate("$namex") == std::vector<Atom>{Text("$namex")});
    CHECK(ParseTemplate("$idx") == std::vector<Atom>{Text("$idx")});
    CHECK((ParseTemplate("$id$serverid") == std::vector<Atom>{Of(Tag::Id), Of(Tag::ServerId)}));
    CHECK((ParseTemplate("\xC3\x97$name\xC3\x97") == std::vector<Atom>{Text("\xC3\x97"), Of(Tag::Name), Text("\xC3\x97")}));
    CHECK(ParseTemplate("a$nopeb") == std::vector<Atom>{Text("a$nopeb")});
}
