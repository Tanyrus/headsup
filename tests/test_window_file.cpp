#include "window_file.h"
#include "test.h"

#include <algorithm>
#include <string>
#include <string_view>

using namespace headsup;

namespace
{
    bool SameText(const char* a, const char* b) { return (a == nullptr) == (b == nullptr) && (a == nullptr || std::string_view(a) == b); }
    bool SameByKind(const char* const* a, const char* const* b)
    {
        if ((a == nullptr) != (b == nullptr)) return false;
        for (int k = 0; a != nullptr && k < kPlateKindCount; ++k)
            if (!SameText(a[k], b[k])) return false;
        return true;
    }
    // Every table of two layouts, entry by entry, texts by what they say.
    void CheckSame(const WindowLayout& a, const WindowLayout& b)
    {
        CHECK(a.pages.size() == b.pages.size() && a.cards.size() == b.cards.size() && a.settings.size() == b.settings.size());
        CHECK(a.rows.size() == b.rows.size() && a.options.size() == b.options.size() && a.lists.size() == b.lists.size());
        CHECK_EQ(a.words.size(), b.words.size());
        for (size_t i = 0; i < a.pages.size(); ++i)
        {
            const LayoutPage &x = a.pages[i], &y = b.pages[i];
            CHECK(x.page == y.page && SameText(x.key, y.key) && SameText(x.name, y.name) && x.child == y.child);
            CHECK(x.firstCard == y.firstCard && x.cardCount == y.cardCount);
        }
        for (size_t i = 0; i < a.cards.size(); ++i)
        {
            const LayoutCard &x = a.cards[i], &y = b.cards[i];
            CHECK(x.piece == y.piece && SameText(x.key, y.key) && SameText(x.name, y.name) && SameText(x.about, y.about));
            CHECK(x.kinds == y.kinds && x.firstSetting == y.firstSetting && x.settingCount == y.settingCount && x.switched == y.switched);
        }
        for (size_t i = 0; i < a.settings.size(); ++i)
        {
            const LayoutSetting &x = a.settings[i], &y = b.settings[i];
            CHECK(x.piece == y.piece && SameText(x.key, y.key) && SameText(x.name, y.name) && SameText(x.tip, y.tip));
            CHECK(SameByKind(x.nameByKind, y.nameByKind) && SameByKind(x.tipByKind, y.tipByKind));
            CHECK(x.kinds == y.kinds && x.entity == y.entity && x.rowsFixed == y.rowsFixed && x.codeKinds == y.codeKinds);
            CHECK_EQ(x.indent, y.indent);
            CHECK(x.firstOption == y.firstOption && x.optionCount == y.optionCount && x.firstRow == y.firstRow && x.rowCount == y.rowCount);
        }
        for (size_t i = 0; i < a.rows.size(); ++i)
        {
            const LayoutRow &x = a.rows[i], &y = b.rows[i];
            CHECK(x.row == y.row && SameText(x.key, y.key) && SameText(x.name, y.name) && SameText(x.tip, y.tip));
            CHECK(SameByKind(x.nameByKind, y.nameByKind) && SameByKind(x.tipByKind, y.tipByKind));
            CHECK(SameText(x.otherTip, y.otherTip) && x.kinds == y.kinds);
        }
        for (size_t i = 0; i < a.options.size(); ++i)
            CHECK(SameText(a.options[i].name, b.options[i].name) && SameText(a.options[i].tip, b.options[i].tip) &&
                  SameText(a.options[i].otherTip, b.options[i].otherTip));
        for (size_t i = 0; i < a.lists.size(); ++i)
            CHECK(a.lists[i].list == b.lists[i].list && SameText(a.lists[i].key, b.lists[i].key) &&
                  a.lists[i].firstOption == b.lists[i].firstOption && a.lists[i].optionCount == b.lists[i].optionCount);
        for (size_t i = 0; i < a.words.size(); ++i)
            CHECK(a.words[i].word == b.words[i].word && SameText(a.words[i].name, b.words[i].name) &&
                  SameText(a.words[i].tip, b.words[i].tip) && SameText(a.words[i].key, b.words[i].key) &&
                  SameText(a.words[i].setting, b.words[i].setting));
    }

    std::string Shipped() { return std::string(kWindowFileText); }
    std::string Replaced(std::string text, std::string_view from, std::string_view to)
    {
        const size_t at = text.find(from);
        CHECK(at != std::string::npos);
        return text.replace(at, from.size(), to);
    }
    // The whole line that holds part, with its end.
    std::string LineWith(const std::string& text, std::string_view part)
    {
        const size_t at = text.find(part);
        CHECK(at != std::string::npos);
        const size_t start = text.rfind('\n', at) + 1, end = text.find('\n', at) + 1;
        return text.substr(start, end - start);
    }
    // The line that holds part, moved to just after the line that holds after.
    std::string Moved(const std::string& text, std::string_view part, std::string_view after)
    {
        const std::string line = LineWith(text, part);
        std::string out        = Replaced(text, line, "");
        const std::string at   = LineWith(out, after);
        return Replaced(out, at, at + line);
    }
    std::string Problem(const std::string& text)
    {
        OwnedLayout owned;
        return ReadWindowFile(text, kWindowLayout, kWindowFileVersion, owned);
    }
    bool Has(const std::string& text, std::string_view part) { return text.find(part) != std::string::npos; }
}

TEST(the_built_in_file_reads_back_as_the_built_in_tables)
{
    OwnedLayout owned;
    CHECK_EQ(ReadWindowFile(kWindowFileText, kWindowLayout, kWindowFileVersion, owned), "");
    CheckSame(owned.View(), kWindowLayout);
}

TEST(a_setting_the_file_indents_reads_back_indented_and_an_indent_is_true_or_false)
{
    const std::string shipped = Shipped();
    const std::string stack   = R"({"key": "stack-names", "name": "Stack Names")";
    OwnedLayout owned;
    CHECK_EQ(ReadWindowFile(Replaced(shipped, stack, stack + R"(, "indent": true)"), kWindowLayout, kWindowFileVersion, owned), "");
    const WindowLayout layout = owned.View();
    const auto indented       = [&](std::string_view key) {
        return std::ranges::find_if(layout.settings, [&](const LayoutSetting& s) { return key == s.key; })->indent;
    };
    CHECK(indented("stack-names") && !indented("vertical-offset"));
    CHECK(Has(Problem(Replaced(shipped, stack, stack + R"(, "indent": "yes")")), "the indent of \"stack-names\" must be true or false"));
}

TEST(a_file_reads_back_in_its_own_order_and_words)
{
    std::string text = Moved(Shipped(), R"({"key": "min-size")", R"({"key": "thickness")");
    text             = Replaced(text, R"("name": "Stack Names")", R"("name": "Stack Them")");
    OwnedLayout owned;
    CHECK_EQ(ReadWindowFile(text, kWindowLayout, kWindowFileVersion, owned), "");
    const WindowLayout layout = owned.View();
    const auto keysOf         = [&](const char* card) {
        std::string keys;
        const auto found = std::ranges::find_if(layout.cards, [&](const LayoutCard& c) { return std::string_view(c.key) == card; });
        for (const LayoutSetting& s : layout.SettingsOf(*found))
            keys += std::string(s.key) + " ";
        return keys;
    };
    CHECK(Has(keysOf("outline-style"), "thickness min-size "));
    CHECK(!Has(keysOf("nameplates"), "min-size"));
    CHECK_EQ(std::string(layout.SettingOf(SettingPiece::StackNames)->name), "Stack Them");
    CHECK(layout.SettingOf(SettingPiece::MinSize)->piece == SettingPiece::MinSize);
}

TEST(a_file_of_another_version_or_no_json_is_refused)
{
    const std::string version = Problem(Replaced(Shipped(), R"("version": ")" + std::string(kWindowFileVersion), R"("version": "0.1)"));
    CHECK(Has(version, "0.1") && Has(version, kWindowFileVersion));
    CHECK(Has(Problem("{\"version\": \"0.65\",\n  \"pages\": [}"), "line 2"));
    CHECK(Has(Problem("[]"), "the top level"));
}

TEST(a_file_is_refused_by_the_entry_that_does_not_fit)
{
    const std::string shipped = Shipped();
    CHECK(Has(Problem(Replaced(shipped, R"({"key": "stack-names")", R"({"key": "stack-nams")")), "\"stack-nams\" is no key of any setting"));
    const std::string minSize = LineWith(shipped, R"({"key": "min-size")");
    CHECK(Has(Problem(Replaced(shipped, minSize, minSize + minSize)), "the setting \"min-size\" is used twice"));
    CHECK(Has(Problem(Replaced(shipped, LineWith(shipped, R"({"key": "stack-names")"), "")), "the setting \"stack-names\" is left out"));
    CHECK(Has(Problem(Replaced(shipped, LineWith(shipped, R"({"tip": "Dot"})"), "")), "\"replace-dashes\" lists 3 options, and the control has 4"));
    CHECK(Has(Problem(Moved(shipped, R"({"key": "aggression", "name": "Aggression")", R"({"key": "links")")),
        "the rows of \"detection\" come in the code's order"));
    CHECK(Has(Problem(Moved(shipped, R"({"key": "no-target")", R"({"key": "min-size")")),
        "\"no-target\" is an entity's design and can only go in the entity cards"));
    CHECK(Has(Problem(Replaced(shipped, R"("key": "claim-colors",)", R"("key": "claim-colors", "name": "Claims",)")),
        "the setting \"claim-colors\" has a name, and the code shows none"));
    CHECK(Has(Problem(Replaced(shipped, R"({"key": "switch-on", "name": "On"})", R"({"key": "switch-on"})")), "the word \"switch-on\" has no name"));
    CHECK(Has(Problem(Replaced(shipped, R"("name": "Stack Names")", R"("name": "Stack##Names")")), "holds \"##\""));
    CHECK(Has(Problem(Replaced(shipped, R"({"key": "stack-names", "name")", R"({"key": "stack-names", "label")")),
        "the setting \"stack-names\" has an unknown field \"label\""));
}
