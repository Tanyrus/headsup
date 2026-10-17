#include "json_text.h"
#include "test.h"

#include <string>
#include <vector>

using namespace headsup;

namespace
{
    std::string Problem(std::string_view text)
    {
        std::string problem;
        CHECK(!ParseJson(text, problem));
        return problem;
    }
    bool Has(const std::string& text, const char* part) { return text.find(part) != std::string::npos; }
}

TEST(an_objects_fields_keep_the_files_order_and_a_list_holds_any_value)
{
    std::string problem;
    const auto value = ParseJson(R"( {"b": 1, "a": [true, false, null, -2.5e1, "x"], "c": {}} )", problem);
    CHECK(value && problem.empty());
    CHECK(value->kind == JsonValue::Kind::Object);
    CHECK((value->names == std::vector<std::string>{"b", "a", "c"}));
    CHECK_EQ(value->Field("b")->number, 1.0);
    const JsonValue& list = *value->Field("a");
    CHECK(list.kind == JsonValue::Kind::List && list.items.size() == 5u);
    CHECK(list.items[0].kind == JsonValue::Kind::Flag && list.items[0].flag && !list.items[1].flag);
    CHECK(list.items[2].kind == JsonValue::Kind::Null);
    CHECK_EQ(list.items[3].number, -25.0);
    CHECK(list.items[4].kind == JsonValue::Kind::Text && list.items[4].text == "x");
    CHECK(value->Field("c")->kind == JsonValue::Kind::Object && value->Field("c")->items.empty());
    CHECK(value->Field("d") == nullptr);
    // A field given twice is its last, as Python's reader takes it.
    CHECK_EQ(ParseJson(R"({"a": 1, "a": 2})", problem)->Field("a")->number, 2.0);
}

TEST(a_strings_escapes_become_its_characters)
{
    std::string problem;
    CHECK_EQ(ParseJson(R"("\"q\" \\ \/ \b\f\n\r\t")", problem)->text, "\"q\" \\ / \b\f\n\r\t");
    CHECK_EQ(ParseJson(R"("café")", problem)->text, "caf\xC3\xA9");
    CHECK_EQ(ParseJson(R"("–")", problem)->text, "\xE2\x80\x93");
    CHECK_EQ(ParseJson(R"("😀")", problem)->text, "\xF0\x9F\x98\x80"); // a pair of halves is one character
    CHECK_EQ(ParseJson("\"caf\xC3\xA9\"", problem)->text, "caf\xC3\xA9");       // and UTF-8 as it is
}

TEST(text_that_is_no_json_is_refused_with_its_line)
{
    CHECK(Has(Problem("{\n  \"a\": 1\n  \"b\": 2\n}"), "line 3"));
    CHECK(Has(Problem("{} x"), "line 1"));
    CHECK(!Problem("{\"a\": [1, 2").empty());
    CHECK(!Problem("").empty());
    CHECK(!Problem(R"(["\q"])").empty());
    CHECK(!Problem("[\"a\tb\"]").empty());       // a control character must be escaped
    CHECK(!Problem(R"(["\ud83d"])").empty());     // half a character
    CHECK(!Problem(R"([tru])").empty());
    CHECK(!Problem(R"([1,])").empty());
    CHECK(!Problem(R"({"a" 1})").empty());
    CHECK(!Problem(R"([01])").empty());
}

TEST(lists_nest_as_deep_as_a_layout_needs_and_no_deeper)
{
    std::string problem;
    CHECK(ParseJson(std::string(kMostJsonDepth, '[') + std::string(kMostJsonDepth, ']'), problem));
    CHECK(!Problem(std::string(kMostJsonDepth + 1, '[') + std::string(kMostJsonDepth + 1, ']')).empty());
}
