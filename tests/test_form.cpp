#include "form.h"
#include "menu_text.h"
#include "test.h"

#include <algorithm>
#include <string>
#include <vector>

using namespace headsup;

namespace
{
    bool Has(const std::string& text, const char* part) { return text.find(part) != std::string::npos; }

    const FormValue* ValueOf(const Form& form, const char* id, FormValueKind kind, const char* name = "")
    {
        for (const FormValue& v : form.values)
            if (v.id == id && v.kind == kind && v.name == name) return &v;
        return nullptr;
    }
    bool ClassOn(const Form& form, const char* id, const char* name)
    {
        const FormValue* v = ValueOf(form, id, FormValueKind::Class, name);
        return v != nullptr && v->on;
    }
    std::string TextOf(const Form& form, const char* id)
    {
        const FormValue* v = ValueOf(form, id, FormValueKind::Text);
        return v != nullptr ? v->text : "<none>";
    }
    std::string ValueText(const Form& form, const char* id)
    {
        const FormValue* v = ValueOf(form, id, FormValueKind::Value);
        return v != nullptr ? v->text : "<none>";
    }

    // One card with a switch, a slider and a dropdown, as a page builds it.
    struct Bound
    {
        bool chocobo  = false;
        bool enabled  = true;
        int height    = 45;
        int theme     = 1;
        float thick   = 1.0f;
        std::string name;
    };
    Form Pass(const std::vector<FormEvent>& events, FormState& state, Bound& b, bool faded = false)
    {
        Form form(events, state);
        form.pageKey = "page";
        if (form.BeginCard("Mouse Pointer", &b.enabled, "A <tip>"))
        {
            form.Fade(faded);
            form.Switch("Chocobo", b.chocobo, "The chocobo.");
            form.SliderInt("Height", b.height, 24, 64, "%d px", nullptr);
            form.SliderFloat("Thickness", b.thick, 0.5f, 3.0f, "%.1f px", nullptr);
            form.NumberDropdown("Theme", b.theme, 0, 2, [](int t) { return "Theme " + std::to_string(t); }, nullptr, 0.0f,
                [](int t) { return "What theme " + std::to_string(t) + " is."; });
            form.Row("Name", nullptr);
            form.TextAt("Name", b.name, "A Name", 9.0f);
            form.EndRow();
            form.Fade(false);
            form.EndCard();
        }
        return form;
    }
}

TEST(a_card_holds_its_rows_and_what_they_show)
{
    FormState state;
    Bound b;
    const Form form = Pass({}, state, b);
    CHECK_EQ(form.blocks.size(), size_t{1});
    CHECK_EQ(form.blocks[0].id, "page-mouse-pointer");
    const std::string& rml = form.blocks[0].rml;
    // What the card is for is written under its title, first in its body, and is no tooltip.
    CHECK(Has(rml, "<div class=\"card-head\" id=\"page-mouse-pointer-head\">"));
    CHECK(Has(rml, "<div class=\"card-body\"><div class=\"note\">A &lt;tip&gt;</div>"));
    CHECK(Has(rml, "<div class=\"arrow open\"></div><div class=\"card-title\">Mouse Pointer</div>"));
    CHECK(Has(rml, "<div class=\"label\" id=\"page-mouse-pointer-chocobo-label\" tip=\"The chocobo.\">Chocobo</div>"));
    CHECK(Has(rml, "<div class=\"check\" id=\"page-mouse-pointer-chocobo\"><div class=\"mark\"></div></div>"));
    CHECK(Has(rml, "<input type=\"range\" id=\"page-mouse-pointer-height\" min=\"24\" max=\"64\" step=\"1\"/>"));
    CHECK(Has(rml, "min=\"0.5\" max=\"3\" step=\"0.1\"")); // a float slider steps by its format's last decimal
    // What they show is no part of the markup.
    CHECK(!ClassOn(form, "page-mouse-pointer-chocobo", "on") && ClassOn(form, "page-mouse-pointer-on", "on"));
    CHECK_EQ(ValueText(form, "page-mouse-pointer-height"), "45");
    CHECK_EQ(TextOf(form, "page-mouse-pointer-height-val"), "45 px");
    CHECK_EQ(TextOf(form, "page-mouse-pointer-thickness-val"), "1.0 px");
    CHECK_EQ(TextOf(form, "page-mouse-pointer-theme-val"), "Theme 1");
    CHECK(!form.changed && !form.popup);
}

TEST(a_click_flips_a_switch_and_the_markup_stays_the_same)
{
    FormState state;
    Bound b;
    const std::string before = Pass({}, state, b).blocks[0].rml;
    const Form form = Pass({{"page-mouse-pointer-chocobo", kClickEvent, ""}}, state, b);
    CHECK(b.chocobo && form.changed);
    CHECK(ClassOn(form, "page-mouse-pointer-chocobo", "on"));
    CHECK_EQ(form.blocks[0].rml, before);
    // The card's own checkbox, at its title's right.
    const Form off = Pass({{"page-mouse-pointer-on", kClickEvent, ""}}, state, b);
    CHECK(!b.enabled && off.changed && !ClassOn(off, "page-mouse-pointer-on", "on"));
}

TEST(a_click_on_a_cards_title_folds_it)
{
    FormState state;
    Bound b;
    const Form folded = Pass({{"page-mouse-pointer-head", kClickEvent, ""}}, state, b);
    CHECK(state.collapsed.contains("page-mouse-pointer"));
    CHECK(!Has(folded.blocks[0].rml, "card-body") && !Has(folded.blocks[0].rml, "chocobo"));
    CHECK(!Has(folded.blocks[0].rml, "tip&gt;")); // folded, it is its title alone
    CHECK(Has(folded.blocks[0].rml, "<div class=\"arrow\"></div>") && !folded.changed);
    const Form open = Pass({{"page-mouse-pointer-head", kClickEvent, ""}}, state, b);
    CHECK(state.collapsed.empty() && Has(open.blocks[0].rml, "card-body"));
}

TEST(a_slider_takes_its_newest_value_within_its_ends)
{
    FormState state;
    Bound b;
    Form form = Pass({{"page-mouse-pointer-height", kChangeEvent, "50"}, {"page-mouse-pointer-height", kChangeEvent, "52.4"}},
        state, b);
    CHECK(b.height == 52 && form.changed);
    CHECK_EQ(TextOf(form, "page-mouse-pointer-height-val"), "52 px");
    Pass({{"page-mouse-pointer-height", kChangeEvent, "900"}}, state, b);
    CHECK_EQ(b.height, 64);
    Pass({{"page-mouse-pointer-thickness", kChangeEvent, "2.5"}}, state, b);
    CHECK_EQ(b.thick, 2.5f);
    // The same value again is no change.
    const Form same = Pass({{"page-mouse-pointer-height", kChangeEvent, "64"}}, state, b);
    CHECK(!same.changed);
}

TEST(a_dropdown_opens_a_popup_of_its_rows_and_takes_the_one_clicked)
{
    FormState state;
    Bound b;
    const Form open = Pass({{"page-mouse-pointer-theme", kClickEvent, ""}}, state, b);
    CHECK_EQ(state.popup, "page-mouse-pointer-theme");
    CHECK(open.popup && open.popup->anchor == "page-mouse-pointer-theme");
    // Each row says what it does under the pointer, and the closed control says it of the one picked.
    CHECK(Has(open.popup->rml, "<div class=\"opt\" id=\"page-mouse-pointer-theme-o-2\" tip=\"What theme 2 is.\">Theme 2</div>"));
    CHECK(Has(open.blocks[0].rml, "<div class=\"dd\" id=\"page-mouse-pointer-theme\" tip=\"What theme 1 is.\""));
    CHECK(ClassOn(open, "page-mouse-pointer-theme-o-1", "picked") && !ClassOn(open, "page-mouse-pointer-theme-o-2", "picked"));
    CHECK(ClassOn(open, "page-mouse-pointer-theme", "open") && !open.changed);
    const Form chose = Pass({{"page-mouse-pointer-theme-o-2", kClickEvent, ""}}, state, b);
    CHECK(b.theme == 2 && chose.changed && state.popup.empty() && !chose.popup);
    // The closed control reads the new choice from the next pass on.
    const Form after = Pass({}, state, b);
    CHECK_EQ(TextOf(after, "page-mouse-pointer-theme-val"), "Theme 2");
}

TEST(a_click_elsewhere_closes_the_popup_and_its_own_control_toggles_it)
{
    FormState state;
    Bound b;
    Pass({{"page-mouse-pointer-theme", kClickEvent, ""}}, state, b);
    const Form away = Pass({{"page-mouse-pointer-chocobo", kClickEvent, ""}}, state, b);
    CHECK(state.popup.empty() && !away.popup && b.chocobo);
    Pass({{"page-mouse-pointer-theme", kClickEvent, ""}}, state, b);
    CHECK(!state.popup.empty());
    Pass({{"page-mouse-pointer-theme", kClickEvent, ""}}, state, b);
    CHECK(state.popup.empty());
}

TEST(a_text_box_takes_its_text_when_it_is_left)
{
    FormState state;
    Bound b;
    // Typing alone changes nothing: the box is committed when it loses the keyboard.
    Pass({{"page-mouse-pointer-name", kChangeEvent, "Ab"}}, state, b);
    CHECK(b.name.empty());
    Pass({{"page-mouse-pointer-name", kCommitEvent, "Abc"}}, state, b);
    CHECK_EQ(b.name, "Abc");
    const Form form = Pass({}, state, b);
    CHECK_EQ(ValueText(form, "page-mouse-pointer-name"), "Abc");
    CHECK(Has(form.blocks[0].rml, "placeholder=\"A Name\""));
}

TEST(what_is_faded_is_marked_off_and_takes_no_clicks)
{
    FormState state;
    Bound b;
    const Form form = Pass({{"page-mouse-pointer-chocobo", kClickEvent, ""},
                               {"page-mouse-pointer-height", kChangeEvent, "30"}},
        state, b, true);
    CHECK(!b.chocobo && b.height == 45 && !form.changed);
    CHECK(ClassOn(form, "page-mouse-pointer-chocobo", "off") && ClassOn(form, "page-mouse-pointer-chocobo-label-row", "off"));
    CHECK(ClassOn(form, "page-mouse-pointer-height-box", "off") && ClassOn(form, "page-mouse-pointer-height-label-row", "off"));
    // The card itself was begun before the fade: it shows.
    CHECK(!ClassOn(form, "page-mouse-pointer", "off"));
    // The card's title was written before the fade began.
    CHECK(!ClassOn(form, "page-mouse-pointer-head", "off"));
    const Form plain = Pass({}, state, b);
    CHECK_EQ(plain.blocks[0].rml, form.blocks[0].rml); // fading replaces no markup
}

TEST(a_card_begun_while_faded_is_off_whole_and_has_no_rows)
{
    FormState state;
    bool on = false, enabled = true;
    Form form({{"p-c-on", kClickEvent, ""}, {"p-c-head", kClickEvent, ""}}, state);
    form.pageKey = "p";
    form.Fade(true);
    CHECK(!form.BeginCard("C", &enabled, "What it is for."));
    CHECK(ClassOn(form, "p-c", "off"));
    CHECK(enabled && !form.changed && state.collapsed.empty()); // and takes no clicks
    CHECK(!Has(form.blocks[0].rml, "card-body"));
    CHECK_EQ(form.CardsShown(), 0);
    form.Fade(false);
    CHECK(form.BeginCard("D"));
    form.Switch("S", on, nullptr);
    form.EndCard();
    CHECK(!ClassOn(form, "p-d", "off"));
    CHECK_EQ(form.CardsShown(), 1);
}

TEST(two_controls_of_one_name_get_ids_of_their_own)
{
    FormState state;
    Form form({}, state);
    form.pageKey = "p";
    form.BeginCard("C");
    bool a = false, b = true;
    form.Switch("Same", a, nullptr);
    form.Switch("Same", b, nullptr);
    form.Switch("Same##other", b, nullptr);
    form.EndCard();
    CHECK(!ClassOn(form, "p-c-same", "on") && ClassOn(form, "p-c-same-2", "on") && ClassOn(form, "p-c-same-other", "on"));
    // What follows "##" is not shown.
    CHECK(Has(form.blocks[0].rml, ">Same</div>") && !Has(form.blocks[0].rml, "##"));
}

TEST(segments_and_tabs_mark_the_picked_one_and_take_a_click)
{
    FormState state;
    const char* const options[] = {"hide", "before", "after"};
    int picked = 2, tab = 0;
    const auto pass = [&](const std::vector<FormEvent>& events) {
        Form form(events, state);
        form.pageKey = "p";
        form.BeginCard("C");
        form.Row("One line", nullptr);
        form.SegmentsAt("One line", picked, options, 3);
        form.EndRow();
        tab = form.Tabs("tabs", tab, options, 3, 2);
        form.EndCard();
        return form;
    };
    const Form first = pass({});
    CHECK(Has(first.blocks[0].rml, "<div class=\"seg\" id=\"p-c-one-line-s-0\" style=\"width:4.6em;\">Hide</div>"));
    CHECK(ClassOn(first, "p-c-one-line-s-2", "picked") && !ClassOn(first, "p-c-one-line-s-0", "picked"));
    // Three tabs, two to a row: a row of two and a row of one.
    CHECK(Has(first.blocks[0].rml, "<div class=\"tab-row\"><div class=\"tab\" id=\"p-c-tabs-t-0\">hide</div>"
                                   "<div class=\"tab\" id=\"p-c-tabs-t-1\">before</div></div><div class=\"tab-row\">"
                                   "<div class=\"tab\" id=\"p-c-tabs-t-2\">after</div></div><div class=\"tab-rule\"></div>"));
    const Form clicked = pass({{"p-c-one-line-s-0", kClickEvent, ""}, {"p-c-tabs-t-2", kClickEvent, ""}});
    CHECK(picked == 0 && clicked.changed && tab == 2);
    CHECK(ClassOn(clicked, "p-c-tabs-t-2", "picked") && ClassOn(clicked, "p-c-one-line-s-0", "picked"));
}

TEST(joined_toggles_each_switch_a_bit_of_their_own)
{
    FormState state;
    uint8_t bits = 0b100;
    const char* const labels[] = {"One", "Two", "Three"};
    const auto pass = [&](const std::vector<FormEvent>& events) {
        Form form(events, state);
        form.pageKey = "p";
        form.BeginCard("C");
        form.Toggles("Style", bits, labels, 3, nullptr);
        form.EndCard();
        return form;
    };
    const Form first = pass({});
    CHECK(!ClassOn(first, "p-c-style-s-0", "picked") && ClassOn(first, "p-c-style-s-2", "picked") && !first.changed);
    const Form on = pass({{"p-c-style-s-0", kClickEvent, ""}});
    CHECK(on.changed && bits == 0b101);
    CHECK(ClassOn(on, "p-c-style-s-0", "picked") && ClassOn(on, "p-c-style-s-2", "picked"));
    const Form off = pass({{"p-c-style-s-2", kClickEvent, ""}});
    CHECK(off.changed && bits == 0b001 && !ClassOn(off, "p-c-style-s-2", "picked"));
    CHECK_EQ(off.blocks[0].rml, first.blocks[0].rml); // a toggle replaces no markup
}

TEST(a_swatch_opens_three_sliders_and_the_color_as_written)
{
    FormState state;
    Color color = ColorFromRgb(0x336699);
    const auto pass = [&](const std::vector<FormEvent>& events) {
        Form form(events, state);
        form.pageKey = "p";
        form.BeginCard("C");
        form.Swatch("Edge", color, nullptr);
        form.EndCard();
        return form;
    };
    const Form closed = pass({});
    const FormValue* shown = ValueOf(closed, "p-c-edge", FormValueKind::Property, "background-color");
    CHECK(shown != nullptr && shown->text == "#336699ff" && !closed.popup);
    const Form open = pass({{"p-c-edge", kClickEvent, ""}});
    CHECK(open.popup && open.popup->anchor == "p-c-edge");
    CHECK(Has(open.popup->rml, "id=\"p-c-edge-p-g\" min=\"0\" max=\"255\" step=\"1\""));
    CHECK_EQ(ValueText(open, "p-c-edge-p-g"), "102");
    CHECK_EQ(ValueText(open, "p-c-edge-p-hex"), "336699");
    const Form moved = pass({{"p-c-edge-p-r", kChangeEvent, "255"}});
    CHECK(color == ColorFromRgb(0xFF6699) && moved.changed && moved.popup);
    pass({{"p-c-edge-p-hex", kCommitEvent, "#102030"}});
    CHECK(color == ColorFromRgb(0x102030));
    pass({{"p-c-edge-p-hex", kCommitEvent, "nonsense"}});
    CHECK(color == ColorFromRgb(0x102030));
}

TEST(a_list_of_checks_stays_open_and_a_condition_unchecked_under_always_turns_always_off)
{
    FormState state;
    LineShow show;
    show.always = false;
    const std::vector<ShowCondition> conditions = ConditionsFor(PlateKind::Mobs);
    const auto pass = [&](const std::vector<FormEvent>& events) {
        Form form(events, state);
        form.pageKey = "p";
        form.BeginCard("C");
        form.ChecksDropdown("Show", show, conditions, nullptr);
        form.EndCard();
        return form;
    };
    pass({{"p-c-show", kClickEvent, ""}});
    const Form checked = pass({{"p-c-show-o-1", kClickEvent, ""}});
    CHECK(show.when == ConditionBit(conditions[0]) && checked.changed && checked.popup);
    CHECK(ClassOn(checked, "p-c-show-o-1", "on") && !ClassOn(checked, "p-c-show-o-2", "on"));
    // Each row says when it holds.
    CHECK(Has(checked.popup->rml, (std::string("id=\"p-c-show-o-1\" tip=\"") + ConditionTip(conditions[0]) + "\"").c_str()));
    CHECK(Has(checked.popup->rml, (std::string("id=\"p-c-show-o-0\" tip=\"") + kShowAlwaysTip + "\"").c_str()));
    // Under Always every condition reads checked.
    const Form always = pass({{"p-c-show-o-0", kClickEvent, ""}});
    CHECK(show.always && ClassOn(always, "p-c-show-o-0", "on") && ClassOn(always, "p-c-show-o-2", "on"));
    CHECK_EQ(TextOf(always, "p-c-show-val"), kShowAlways);
    // Unchecking one of them turns Always off and leaves the others checked.
    const Form one = pass({{"p-c-show-o-2", kClickEvent, ""}});
    CHECK(one.changed && !show.always && one.popup);
    CHECK(!ClassOn(one, "p-c-show-o-0", "on") && !ClassOn(one, "p-c-show-o-2", "on") && ClassOn(one, "p-c-show-o-3", "on"));
    CHECK_EQ(TextOf(one, "p-c-show-val"), std::to_string(conditions.size() - 1) + " Conditions");
}

TEST(a_line_box_takes_typing_a_dropped_tag_and_its_caret)
{
    FormState state;
    std::string text = "$name";
    LineDraft draft;
    const auto pass = [&](const std::vector<FormEvent>& events) {
        Form form(events, state);
        form.pageKey = "p";
        form.BeginCard("C");
        const bool edited = form.LineBox("line", text, draft);
        const bool chip   = form.TagChip(Tag::Level);
        form.EndCard();
        return std::pair{form.changed, edited || chip};
    };
    const auto focused = pass({{"p-c-line", kFocusEvent, ""}});
    CHECK(!focused.first && draft.typing);
    const auto typed = pass({{"p-c-line", kCaretEvent, "2"}, {"p-c-line", kCommitEvent, "Lv $level"}});
    CHECK(typed.first && text == "Lv $level" && draft.caret == 2 && !draft.typing);
    // A tag chip dropped on the box goes at its end, as typed there.
    const auto dropped = pass({{"p-c-line", kDropEvent, "tag-" + std::to_string(static_cast<int>(Tag::Name))}});
    CHECK(dropped.first);
    CHECK_EQ(text, "Lv $level$name");
    CHECK_EQ(draft.caret, text.size());
    const auto clicked = pass({{"tag-" + std::to_string(static_cast<int>(Tag::Level)), kClickEvent, ""}});
    CHECK(clicked.second);
}
