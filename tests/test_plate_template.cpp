#include "plate_template.h"
#include "test.h"

#include <algorithm>

using namespace headsup;

namespace
{
    Atom Text(const char* t) { return Atom{false, Tag::Name, t}; }
    Atom Of(Tag tag) { return Atom{true, tag, ""}; }
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

TEST(a_tag_is_put_in_at_the_caret_or_the_end)
{
    CHECK_EQ(InsertTag("Lv  EP", Tag::Level, 3), "Lv $level EP");
    CHECK_EQ(InsertTag("Lv", Tag::Level, 99), "Lv$level");
    CHECK_EQ(InsertTag("", Tag::Name, 0), "$name");
    const std::string full(kMaxTemplate - 2, 'x');
    CHECK_EQ(InsertTag(full, Tag::Name, 0), full); // half a tag is a design nobody wrote: the insert is refused
    CHECK_EQ(InsertTag(std::string(kMaxTemplate - 5, 'x'), Tag::Name, 0).size(), kMaxTemplate);
}

TEST(a_design_uses_a_tag_when_any_line_holds_it)
{
    KindSettings kind;
    kind.lines[1].text = "Lv $level";
    CHECK(UsesTag(kind, Tag::Level));
    CHECK(!UsesTag(kind, Tag::Con));
}

TEST(the_default_templates_use_only_tags_the_catalog_knows)
{
    for (const char* text : {kTagDetect, kTagWeakResist, kTagStatus, kTagImmune, kTagLevelCon, kTagId, kTagServerId,
             kTagPlaceholder, kTagName, kTagPlayerName})
        for (const Atom& atom : ParseTemplate(text))
            CHECK(atom.isTag || atom.text.find(kTagMark) == std::string::npos);
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
                                              Tag::Linkshell, Tag::Bazaar, Tag::Seeking, Tag::Job, Tag::Level}));
}

TEST(a_tag_name_runs_to_the_last_letter_and_text_around_unknown_tags_stays_whole)
{
    CHECK(ParseTemplate("$namex") == std::vector<Atom>{Text("$namex")});
    CHECK(ParseTemplate("$idx") == std::vector<Atom>{Text("$idx")});
    CHECK((ParseTemplate("$id$serverid") == std::vector<Atom>{Of(Tag::Id), Of(Tag::ServerId)}));
    CHECK((ParseTemplate("\xC3\x97$name\xC3\x97") == std::vector<Atom>{Text("\xC3\x97"), Of(Tag::Name), Text("\xC3\x97")}));
    CHECK(ParseTemplate("a$nopeb") == std::vector<Atom>{Text("a$nopeb")});
}
