#include "ini_text.h"
#include "test.h"

#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

using namespace headsup;

namespace
{
    // The file with the keys set in [settings], the section HeadsUp's file has.
    std::string Set(std::string_view file, std::initializer_list<KeyText> values)
    {
        const std::vector<KeyText> list(values);
        return WithIniValues(file, "settings", list);
    }
}

TEST(a_keys_value_is_replaced_on_its_own_line)
{
    // As Ashita writes the file: the section, then key=value lines, each ended by CR LF.
    const char* file = "[settings]\r\nenabled=true\r\nthickness=1.0000\r\nfontName=Georgia\r\n";
    CHECK_EQ(Set(file, {{"thickness", "6.5000"}}), "[settings]\r\nenabled=true\r\nthickness=6.5000\r\nfontName=Georgia\r\n");
    CHECK_EQ(Set(file, {{"fontName", "Marcellus SC"}, {"enabled", "false"}}),
        "[settings]\r\nenabled=false\r\nthickness=1.0000\r\nfontName=Marcellus SC\r\n");
    CHECK_EQ(Set(file, {}), file);
}

TEST(a_key_the_section_lacks_is_added_at_the_sections_end)
{
    // Before the blank line that sets the next section apart, and in the order given.
    const char* file = "[settings]\r\nenabled=true\r\n\r\n[other]\r\nwidth=3\r\n";
    CHECK_EQ(Set(file, {{"nameplateDesigns", "true"}, {"settingsVersion", "3.0000"}}),
        "[settings]\r\nenabled=true\r\nnameplateDesigns=true\r\nsettingsVersion=3.0000\r\n\r\n[other]\r\nwidth=3\r\n");
    // One key replaced and one added in the same call.
    CHECK_EQ(Set(file, {{"baseSize", "15.0000"}, {"enabled", "false"}}),
        "[settings]\r\nenabled=false\r\nbaseSize=15.0000\r\n\r\n[other]\r\nwidth=3\r\n");
    // A section that is its header alone.
    CHECK_EQ(Set("[settings]\r\n[other]\r\nwidth=3\r\n", {{"enabled", "true"}}),
        "[settings]\r\nenabled=true\r\n[other]\r\nwidth=3\r\n");
}

TEST(only_the_named_sections_key_changes)
{
    // The same key in the section before, the one after, and on a line before any section.
    const char* file = "enabled=global\r\n[boot]\r\nenabled=boot\r\n[settings]\r\nenabled=true\r\n[other]\r\nenabled=other\r\n";
    CHECK_EQ(Set(file, {{"enabled", "false"}}),
        "enabled=global\r\n[boot]\r\nenabled=boot\r\n[settings]\r\nenabled=false\r\n[other]\r\nenabled=other\r\n");
    // The section first of several, and last.
    CHECK_EQ(Set("[settings]\r\nenabled=true\r\n[other]\r\nenabled=other\r\n", {{"enabled", "false"}, {"added", "1"}}),
        "[settings]\r\nenabled=false\r\nadded=1\r\n[other]\r\nenabled=other\r\n");
    CHECK_EQ(Set("[other]\r\nenabled=other\r\n[settings]\r\nenabled=true\r\n", {{"enabled", "false"}, {"added", "1"}}),
        "[other]\r\nenabled=other\r\n[settings]\r\nenabled=false\r\nadded=1\r\n");
}

TEST(everything_else_in_the_file_is_kept_byte_for_byte)
{
    // Comments, blank lines, a key HeadsUp does not know, a line with no value and a key written with spaces.
    const char* file = "; made by hand\r\n\r\n[boot]\r\nfile = x \r\n\r\n[settings]\r\n# a note\r\n;enabled=commented\r\n"
                       "unknownKey= kept as it is \r\nnot a pair\r\n\r\n  thickness = 1.0000\r\nenabled=true\r\n\r\n";
    CHECK_EQ(Set(file, {{"enabled", "false"}}),
        "; made by hand\r\n\r\n[boot]\r\nfile = x \r\n\r\n[settings]\r\n# a note\r\n;enabled=commented\r\n"
        "unknownKey= kept as it is \r\nnot a pair\r\n\r\n  thickness = 1.0000\r\nenabled=false\r\n\r\n");
    // A key matches trimmed, and what stands before its = stays as written.
    CHECK_EQ(Set("[settings]\r\n  thickness = 1.0000\r\n", {{"thickness", "6.5000"}}), "[settings]\r\n  thickness =6.5000\r\n");
}

TEST(a_key_matches_whole_and_exactly)
{
    const char* file = "[settings]\r\nmobsLine1TextSize=100.0000\r\nmobsLine1Text=$detect\r\n";
    CHECK_EQ(Set(file, {{"mobsLine1Text", "$name"}}), "[settings]\r\nmobsLine1TextSize=100.0000\r\nmobsLine1Text=$name\r\n");
    CHECK_EQ(Set(file, {{"mobsLine1TextSize", "87.0000"}}), "[settings]\r\nmobsLine1TextSize=87.0000\r\nmobsLine1Text=$detect\r\n");
    // With only the longer key there, the shorter is added, and the other way round; case counts.
    CHECK_EQ(Set("[settings]\r\nmobsLine1TextSize=100.0000\r\n", {{"mobsLine1Text", "$name"}}),
        "[settings]\r\nmobsLine1TextSize=100.0000\r\nmobsLine1Text=$name\r\n");
    CHECK_EQ(Set("[settings]\r\nmobsLine1Text=$detect\r\n", {{"mobsLine1TextSize", "87.0000"}}),
        "[settings]\r\nmobsLine1Text=$detect\r\nmobsLine1TextSize=87.0000\r\n");
}

TEST(a_key_is_found_whatever_its_case_and_keeps_the_files_spelling)
{
    // The profile reader Ashita loads through compares names without case, so another spelling is the same key.
    CHECK_EQ(Set("[settings]\r\nEnabled=true\r\nBASESIZE=15.0000\r\n", {{"enabled", "false"}, {"baseSize", "22.0000"}}),
        "[settings]\r\nEnabled=false\r\nBASESIZE=22.0000\r\n");
    CHECK_EQ(Set("[settings]\r\nMobsLine1TextSize=100.0000\r\n", {{"mobsLine1Text", "$name"}}),
        "[settings]\r\nMobsLine1TextSize=100.0000\r\nmobsLine1Text=$name\r\n"); // still whole
}

TEST(the_section_is_found_as_the_reader_finds_it)
{
    // Whatever its case, with space around its brackets or inside them, text after them, or a byte order mark before
    // it at the file's start: its line stays as written and no second section is added.
    for (const char* header : {"[Settings]", "[SETTINGS]", "[ settings ]", "  [settings]  ", "[settings] ; mine",
             "\xEF\xBB\xBF[settings]", "\xEF\xBB\xBF [ Settings ] note"})
    {
        const std::string file = std::string(header) + "\r\nenabled=true\r\n[other]\r\nwidth=3\r\n";
        CHECK_EQ(Set(file, {{"enabled", "false"}, {"added", "1"}}),
            std::string(header) + "\r\nenabled=false\r\nadded=1\r\n[other]\r\nwidth=3\r\n");
    }
    // A byte order mark anywhere else is not one, and a name that only holds the section's is another.
    CHECK_EQ(Set("[other]\r\n\xEF\xBB\xBF[settings]\r\nenabled=true\r\n", {{"enabled", "false"}}),
        "[other]\r\n\xEF\xBB\xBF[settings]\r\nenabled=true\r\n[settings]\r\nenabled=false\r\n");
    CHECK_EQ(Set("[my settings]\r\nenabled=true\r\n", {{"enabled", "false"}}),
        "[my settings]\r\nenabled=true\r\n[settings]\r\nenabled=false\r\n");
}

TEST(the_section_ends_at_the_next_header_however_that_one_is_written)
{
    // With text after its bracket, with an = in it (the reader takes the line for a header up to its last bracket),
    // or with no closing bracket at all: the same key under it is another section's, and new keys go before it.
    for (const char* next : {"[other] ; note", "[Other]x", "  [other]", "[a]=[b]", "[broken"})
    {
        const std::string file = std::string("[settings]\r\nenabled=true\r\n") + next + "\r\nenabled=other\r\nwidth=3\r\n";
        CHECK_EQ(Set(file, {{"enabled", "false"}, {"width", "9"}}),
            std::string("[settings]\r\nenabled=false\r\nwidth=9\r\n") + next + "\r\nenabled=other\r\nwidth=3\r\n");
    }
    // A value that looks like a header is a value: its line begins with its key.
    CHECK_EQ(Set("[settings]\r\nmobsLine1Text=[other]\r\nenabled=true\r\n", {{"enabled", "false"}}),
        "[settings]\r\nmobsLine1Text=[other]\r\nenabled=false\r\n");
}

TEST(only_the_first_section_of_the_name_is_changed)
{
    // The reader reads the first and no other: a later one keeps its lines, and new keys go to the first.
    const char* twice = "[settings]\r\nenabled=first\r\na=1\r\n[other]\r\nx=1\r\n[settings]\r\nenabled=second\r\nb=2\r\n";
    CHECK_EQ(Set(twice, {{"enabled", "false"}, {"b", "3"}, {"added", "1"}}),
        "[settings]\r\nenabled=false\r\na=1\r\nb=3\r\nadded=1\r\n[other]\r\nx=1\r\n[settings]\r\nenabled=second\r\nb=2\r\n");
    CHECK_EQ(Set("[Settings]\r\nenabled=first\r\n[settings]\r\nenabled=second\r\n", {{"enabled", "false"}}),
        "[Settings]\r\nenabled=false\r\n[settings]\r\nenabled=second\r\n");
}

TEST(a_text_cannot_start_a_line_of_its_own)
{
    // Each carriage return and line feed becomes a space.
    const char* file = "[settings]\r\nenabled=true\r\n";
    CHECK_EQ(Set(file, {{"mobsLine1Text", "a\nenabled=false"}}), "[settings]\r\nenabled=true\r\nmobsLine1Text=a enabled=false\r\n");
    CHECK_EQ(Set(file, {{"enabled", "x\r\n[boot]\rfile=evil"}}), "[settings]\r\nenabled=x  [boot] file=evil\r\n");
}

TEST(a_key_the_reader_would_not_take_as_one_is_left_out)
{
    // Empty, holding a line break or an =, or beginning as a header or a comment does.
    const char* file = "[settings]\r\n=orphan\r\nenabled=true\r\n";
    CHECK_EQ(Set(file, {{"", "x"}, {"a\nb", "x"}, {"a\rb", "x"}, {"a=b", "x"}, {"[boot]", "x"}, {";enabled", "x"}}), file);
    CHECK_EQ(Set(file, {{"", "x"}, {"baseSize", "15.0000"}, {"a=b", "x"}}), "[settings]\r\n=orphan\r\nenabled=true\r\nbaseSize=15.0000\r\n");
    CHECK_EQ(Set("", {{"a=b", "x"}}), ""); // and no section is made for it
}

TEST(a_key_given_twice_takes_its_last_text)
{
    const char* file = "[settings]\r\nenabled=true\r\n";
    CHECK_EQ(Set(file, {{"enabled", "A"}, {"enabled", "B"}}), "[settings]\r\nenabled=B\r\n");
    CHECK_EQ(Set(file, {{"added", "A"}, {"baseSize", "15.0000"}, {"Added", "B"}}),
        "[settings]\r\nenabled=true\r\nadded=B\r\nbaseSize=15.0000\r\n"); // once, where it was first given
}

namespace
{
    bool Holds(std::string_view file, std::initializer_list<KeyText> values)
    {
        const std::vector<KeyText> list(values);
        return HoldsIniValues(file, "settings", list);
    }
}

TEST(a_file_holds_the_values_when_setting_them_would_change_nothing)
{
    const char* file = "[Settings]\r\nenabled=true\r\nmobsLine1Text=\r\nBaseSize=15.0000\r\n[other]\r\nwidth=3\r\n";
    CHECK(Holds(file, {{"enabled", "true"}}));
    CHECK(Holds(file, {{"enabled", "true"}, {"mobsLine1Text", ""}, {"baseSize", "15.0000"}}));
    CHECK(Holds(file, {}));
    CHECK(!Holds(file, {{"enabled", "false"}}));                      // another text
    CHECK(!Holds(file, {{"enabled", "true"}, {"baseSize", "15"}}));   // one of them spelled otherwise
    CHECK(!Holds(file, {{"width", "3"}}));                            // held by another section only
    CHECK(!Holds(file, {{"nameplateDesigns", "true"}}));              // not there
    CHECK(!Holds("", {{"enabled", "true"}}));                         // no file
    CHECK(!Holds("[other]\r\nenabled=true\r\n", {{"enabled", "true"}})); // no section
    // The key twice, the reader's line right and the other not: the writer would still set the second.
    CHECK(!Holds("[settings]\r\nenabled=true\r\nenabled=false\r\n", {{"enabled", "true"}}));
    CHECK(Holds("[settings]\r\nmobsLine1Text=a b\r\n", {{"mobsLine1Text", "a\nb"}})); // as the writer would write it
}

TEST(a_value_is_written_as_it_is_given)
{
    // A design line holding =, ; and #, quotes and a two-byte letter, over a value that held an = itself.
    const char* file = "[settings]\r\nmobsLine1Text=a=b ; c\r\nbaseSize=15.0000\r\n";
    CHECK_EQ(Set(file, {{"mobsLine1Text", "\"$name\" ; #$id = x \xC3\xA9"}}),
        "[settings]\r\nmobsLine1Text=\"$name\" ; #$id = x \xC3\xA9\r\nbaseSize=15.0000\r\n");
    CHECK_EQ(Set(file, {{"mobsLine1Text", ""}}), "[settings]\r\nmobsLine1Text=\r\nbaseSize=15.0000\r\n");
    CHECK_EQ(Set(file, {{"friends", ""}}), "[settings]\r\nmobsLine1Text=a=b ; c\r\nbaseSize=15.0000\r\nfriends=\r\n");
}

TEST(a_key_written_twice_in_the_section_is_set_on_both_lines)
{
    const char* file = "[settings]\r\nenabled=true\r\nbaseSize=15.0000\r\nenabled=maybe\r\n";
    CHECK_EQ(Set(file, {{"enabled", "false"}}), "[settings]\r\nenabled=false\r\nbaseSize=15.0000\r\nenabled=false\r\n");
}

TEST(new_lines_end_as_the_files_lines_do)
{
    const char* unix = "[settings]\nenabled=true\n\n[other]\nwidth=3\n";
    CHECK_EQ(Set(unix, {{"enabled", "false"}, {"baseSize", "15.0000"}}),
        "[settings]\nenabled=false\nbaseSize=15.0000\n\n[other]\nwidth=3\n");
    CHECK_EQ(Set("[other]\nwidth=3\n", {{"enabled", "true"}}), "[other]\nwidth=3\n[settings]\nenabled=true\n");
}

TEST(a_file_whose_last_line_has_no_ending_gets_one_and_none_is_doubled)
{
    CHECK_EQ(Set("[settings]\r\nenabled=true", {{"enabled", "false"}}), "[settings]\r\nenabled=false\r\n");
    CHECK_EQ(Set("[settings]\r\nenabled=true", {{"baseSize", "15.0000"}}), "[settings]\r\nenabled=true\r\nbaseSize=15.0000\r\n");
    CHECK_EQ(Set("[settings]\nenabled=true", {{"baseSize", "15.0000"}}), "[settings]\nenabled=true\nbaseSize=15.0000\n");
    CHECK_EQ(Set("[other]\r\nwidth=3", {{"enabled", "true"}}), "[other]\r\nwidth=3\r\n[settings]\r\nenabled=true\r\n");
    // A header alone, with no line ending anywhere, is a file with none: CR LF.
    CHECK_EQ(Set("[settings]", {{"enabled", "true"}}), "[settings]\r\nenabled=true\r\n");
    // Blank lines at the file's end stay where they are, after the section's last line.
    CHECK_EQ(Set("[settings]\r\nenabled=true\r\n\r\n", {{"baseSize", "15.0000"}}),
        "[settings]\r\nenabled=true\r\nbaseSize=15.0000\r\n\r\n");
}

TEST(a_file_without_the_section_gets_it_at_its_end_and_an_empty_one_is_the_section_alone)
{
    CHECK_EQ(Set("", {{"enabled", "true"}, {"mobsLine1Text", ""}}), "[settings]\r\nenabled=true\r\nmobsLine1Text=\r\n");
    CHECK_EQ(Set("[other]\r\nenabled=other\r\n", {{"enabled", "true"}}), "[other]\r\nenabled=other\r\n[settings]\r\nenabled=true\r\n");
    CHECK_EQ(Set("[other]\r\nenabled=other\r\n", {}), "[other]\r\nenabled=other\r\n"); // nothing to set, nothing added
    // A section whose name only starts or ends like it is another.
    CHECK_EQ(Set("[settings2]\r\nenabled=other\r\n", {{"enabled", "true"}}),
        "[settings2]\r\nenabled=other\r\n[settings]\r\nenabled=true\r\n");
}

TEST(a_value_is_read_as_the_reader_reads_it)
{
    const char* file = "\xEF\xBB\xBF[Settings]\r\n; root_path=commented\r\nRoot_Path = X:\\DATs \r\nroot_path=second\r\n"
                       "[overlays]\r\n0=A\r\n[settings]\r\nlater=1\r\n";
    CHECK(IniValue(file, "settings", "root_path") == std::optional<std::string>("X:\\DATs"));
    CHECK(IniValue(file, "OVERLAYS", "0") == std::optional<std::string>("A"));
    CHECK(!IniValue(file, "overlays", "1"));
    CHECK(!IniValue(file, "settings", "later")); // only the first section of the name is read
    CHECK(!IniValue(file, "cache", "0"));
    CHECK(IniValue("[a]\nk=\n", "a", "k") == std::optional<std::string>(""));
}
