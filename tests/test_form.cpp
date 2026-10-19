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
        int opacity   = 85;
        int quality   = 1;
        std::string name;
    };
    const char* const kQualities[] = {"Low", "Medium", "High", "Ultra"};
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
            form.SliderNamed("Quality", b.quality, kQualities, 4, nullptr);
            form.SliderPercent("Opacity", b.opacity, 10, 100, nullptr);
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

TEST(a_stepper_moves_one_a_click_between_its_ends_and_shows_its_value)
{
    FormState state;
    int size        = 18;
    const auto pass = [&](const std::vector<FormEvent>& events) {
        Form form(events, state);
        form.pageKey = "p";
        form.BeginCard("C");
        form.Stepper("Size", size, 10, 20, "%d px", nullptr);
        form.EndCard();
        return form;
    };
    const Form first = pass({});
    CHECK(!first.changed && TextOf(first, "p-c-size-val") == "18 px");
    const Form up = pass({{"p-c-size-next", kClickEvent, ""}});
    CHECK(up.changed && size == 19 && TextOf(up, "p-c-size-val") == "19 px");
    CHECK_EQ(up.blocks[0].rml, first.blocks[0].rml); // a step is a value the window sets, and rebuilds nothing
    const Form down = pass({{"p-c-size-previous", kClickEvent, ""}});
    CHECK(down.changed && size == 18);
    size             = 20;
    const Form top   = pass({{"p-c-size-next", kClickEvent, ""}});
    CHECK(!top.changed && size == 20);
    size             = 10;
    const Form floor = pass({{"p-c-size-previous", kClickEvent, ""}});
    CHECK(!floor.changed && size == 10);
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
    CHECK(Has(rml, "<div class=\"check wide\" id=\"page-mouse-pointer-chocobo\"><div class=\"mark\"></div><div class=\"state\" "
                   "id=\"page-mouse-pointer-chocobo-state\"></div></div>"));
    CHECK(Has(rml, "<input type=\"range\" id=\"page-mouse-pointer-height\" min=\"24\" max=\"64\" step=\"1\"/>"));
    CHECK(Has(rml, "min=\"0.5\" max=\"3\" step=\"0.1\"")); // a float slider steps by its format's last decimal
    // What they show is no part of the markup.
    CHECK(!ClassOn(form, "page-mouse-pointer-chocobo", "on") && ClassOn(form, "page-mouse-pointer-on", "on"));
    CHECK_EQ(ValueText(form, "page-mouse-pointer-height"), "45");
    CHECK_EQ(TextOf(form, "page-mouse-pointer-height-val"), "45 px");
    CHECK_EQ(TextOf(form, "page-mouse-pointer-chocobo-state"), "Off"); // the kit's own words, from the layout
    CHECK_EQ(TextOf(form, "page-mouse-pointer-on-state"), "On");
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
    CHECK_EQ(TextOf(form, "page-mouse-pointer-chocobo-state"), "On");
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
    // A slider of names steps through them and shows the one it is on, never its number.
    CHECK_EQ(TextOf(same, "page-mouse-pointer-quality-val"), "Medium");
    const Form named = Pass({{"page-mouse-pointer-quality", kChangeEvent, "3"}}, state, b);
    CHECK(b.quality == 3 && named.changed);
    CHECK_EQ(TextOf(named, "page-mouse-pointer-quality-val"), "Ultra");
    const Form lowest = Pass({{"page-mouse-pointer-quality", kChangeEvent, "-4"}}, state, b);
    CHECK_EQ(b.quality, 0);
    CHECK_EQ(TextOf(lowest, "page-mouse-pointer-quality-val"), "Low");
}

TEST(a_percent_slider_steps_by_five_whatever_value_it_is_given)
{
    FormState state;
    Bound b;
    const Form form = Pass({}, state, b);
    CHECK(Has(form.blocks[0].rml, "id=\"page-mouse-pointer-opacity\" min=\"10\" max=\"100\" step=\"5\""));
    CHECK_EQ(TextOf(form, "page-mouse-pointer-opacity-val"), "85%");
    const Form moved = Pass({{"page-mouse-pointer-opacity", kChangeEvent, "88"}}, state, b);
    CHECK(b.opacity == 90 && moved.changed);
    CHECK_EQ(TextOf(moved, "page-mouse-pointer-opacity-val"), "90%");
    Pass({{"page-mouse-pointer-opacity", kChangeEvent, "12"}}, state, b);
    CHECK_EQ(b.opacity, 10);
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
    const FormValue* said = ValueOf(open, "page-mouse-pointer-theme", FormValueKind::Attribute, "tip");
    CHECK(said != nullptr && said->text == "What theme 1 is.");
    CHECK(ClassOn(open, "page-mouse-pointer-theme-o-1", "picked") && !ClassOn(open, "page-mouse-pointer-theme-o-2", "picked"));
    CHECK(ClassOn(open, "page-mouse-pointer-theme", "open") && !open.changed);
    const Form chose = Pass({{"page-mouse-pointer-theme-o-2", kClickEvent, ""}}, state, b);
    CHECK(b.theme == 2 && chose.changed && state.popup.empty() && !chose.popup);
    // The closed control reads the new choice from the next pass on, and says what it does, with the card's
    // markup as it was: a pick builds nothing anew.
    const Form after = Pass({}, state, b);
    CHECK_EQ(TextOf(after, "page-mouse-pointer-theme-val"), "Theme 2");
    CHECK_EQ(ValueOf(after, "page-mouse-pointer-theme", FormValueKind::Attribute, "tip")->text, "What theme 2 is.");
    CHECK_EQ(after.blocks[0].rml, open.blocks[0].rml);
}

TEST(what_is_faded_is_marked_off_by_value_and_its_card_is_built_the_same)
{
    FormState state;
    int value = 3;
    const auto pass = [&](bool off) {
        Form form({}, state);
        form.pageKey = "p";
        form.BeginCard("C");
        form.Fade(off);
        form.Caption("Top Line", "line-name");
        form.Note("Something to say.");
        form.NumberAt("X", "x", value, 300);
        form.EndCard();
        return form;
    };
    const Form shown = pass(false), faded = pass(true);
    CHECK_EQ(faded.blocks[0].rml, shown.blocks[0].rml);
    int off = 0;
    for (const FormValue& v : faded.values)
        off += v.kind == FormValueKind::Class && v.name == kOffClass && v.on ? 1 : 0;
    CHECK(off >= 4); // the caption, the note, the number's name and its box
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

TEST(a_number_box_takes_a_whole_number_within_its_limit_and_nothing_else)
{
    FormState state;
    int value = 7;
    const auto pass = [&](const char* typed) {
        Form form({{"p-c-x", kCommitEvent, typed}}, state);
        form.pageKey = "p";
        form.BeginCard("C");
        form.NumberAt("X", "x", value, 300);
        form.EndCard();
        return form.changed;
    };
    CHECK(pass("120") && value == 120);
    CHECK(!pass("120")); // the same again is no change
    CHECK(pass("-45") && value == -45);
    CHECK(pass("99999") && value == 300); // past its limit, either way, is the limit
    CHECK(pass("-99999") && value == -300);
    CHECK(!pass("abc") && value == -300); // no number: as it was
    CHECK(!pass("") && value == -300);
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
    CHECK(!ClassOn(form, "p-c-same", "on") && ClassOn(form, "p-c-same-2", "on") && ClassOn(form, "p-c-other", "on"));
    // What follows "##" is not shown.
    CHECK(Has(form.blocks[0].rml, ">Same</div>") && !Has(form.blocks[0].rml, "##"));
}

TEST(what_follows_two_hashes_names_the_id_and_what_comes_before_is_shown)
{
    FormState state;
    Form form({}, state);
    form.pageKey = "p";
    CHECK(form.BeginCard("A Title##t"));
    form.Action("Do It##do");
    form.Caption("Head##h");
    form.EndCard();
    const std::string& rml = form.blocks[0].rml;
    CHECK_EQ(form.blocks[0].id, std::string("p-t"));
    CHECK(Has(rml, "id=\"p-t-head\"") && Has(rml, ">A Title</div>"));
    CHECK(Has(rml, "id=\"p-t-do\"") && Has(rml, ">Do It</div>"));
    CHECK(Has(rml, "id=\"p-t-h-caption\"") && Has(rml, ">Head</div>"));
    CHECK(!Has(rml, "##"));
}

TEST(a_card_that_shows_nothing_goes_unless_its_title_has_a_switch)
{
    FormState state;
    const auto pass = [&](bool* enabled, bool note) {
        Form form({}, state);
        form.pageKey = "p";
        form.BeginCard("C", enabled);
        bool on = false;
        form.Fade(true);
        form.Switch("Hidden", on, nullptr);
        form.Fade(false);
        if (note) form.Note("Shown.");
        form.EndCard();
        return form;
    };
    const Form empty = pass(nullptr, false);
    CHECK(ClassOn(empty, "p-c", kOffClass) && empty.CardsShown() == 0);
    bool enabled = false;
    const Form switched = pass(&enabled, false);
    CHECK(!ClassOn(switched, "p-c", kOffClass) && switched.CardsShown() == 1);
    const Form noted = pass(nullptr, true);
    CHECK(!ClassOn(noted, "p-c", kOffClass) && noted.CardsShown() == 1);
}

TEST(segments_and_tabs_mark_the_picked_one_and_take_a_click)
{
    FormState state;
    const char* const options[] = {"Hide", "Before", "After"};
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
    CHECK(Has(first.blocks[0].rml, "<div class=\"tab-row\"><div class=\"tab\" id=\"p-c-tabs-t-0\">Hide</div>"
                                   "<div class=\"tab\" id=\"p-c-tabs-t-1\">Before</div></div><div class=\"tab-row\">"
                                   "<div class=\"tab\" id=\"p-c-tabs-t-2\">After</div></div><div class=\"tab-rule\"></div>"));
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
    // A slider says its value as it is made and each time it is set. Said back as it stands, a level is not the
    // player's: a color between two levels, as 0.15 is, stays the color it was.
    color = Color{{1.0f, 0.15f, 0.15f}};
    const Form echoed = pass({{"p-c-edge-p-r", kChangeEvent, "255"}, {"p-c-edge-p-g", kChangeEvent, "38"}, {"p-c-edge-p-b", kChangeEvent, "38"}});
    CHECK(!echoed.changed && (color == Color{{1.0f, 0.15f, 0.15f}}));
    const Form nudged = pass({{"p-c-edge-p-g", kChangeEvent, "39"}});
    CHECK(nudged.changed && color.v[1] == 39.0f / 255.0f && color.v[2] == 0.15f);
}

TEST(a_color_with_alpha_opens_four_sliders_and_its_hex_keeps_the_alpha)
{
    FormState state;
    uint32_t argb   = 0x80336699;
    const auto pass = [&](const std::vector<FormEvent>& events) {
        Form form(events, state);
        form.pageKey = "p";
        form.BeginCard("C");
        form.ArgbSwatch("Fill", argb, nullptr);
        form.EndCard();
        return form;
    };
    const Form closed      = pass({});
    const FormValue* shown = ValueOf(closed, "p-c-fill", FormValueKind::Property, "background-color");
    CHECK(shown != nullptr && shown->text == "#33669980");
    const Form open = pass({{"p-c-fill", kClickEvent, ""}});
    CHECK(open.popup && ValueText(open, "p-c-fill-p-a") == "128" && ValueText(open, "p-c-fill-p-hex") == "336699");
    const Form opaque = pass({{"p-c-fill-p-a", kChangeEvent, "255"}});
    CHECK(opaque.changed && argb == 0xFF336699u);
    const Form green = pass({{"p-c-fill-p-g", kChangeEvent, "0"}});
    CHECK(green.changed && argb == 0xFF330099u);
    pass({{"p-c-fill-p-a", kChangeEvent, "64"}});
    pass({{"p-c-fill-p-hex", kCommitEvent, "#102030"}});
    CHECK_EQ(argb, 0x40102030u);
    const Form echoed = pass({{"p-c-fill-p-r", kChangeEvent, "16"}}); // said back as it stands
    CHECK(!echoed.changed);
}

TEST(a_list_of_checks_stays_open_and_a_condition_unchecked_under_always_turns_always_off)
{
    FormState state;
    LineShow show;
    show.always = false;
    const std::span<const ShowCondition> conditions = ConditionsFor(PlateKind::Mobs);
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
    CHECK(Has(checked.popup->rml, (std::string("id=\"p-c-show-o-1\" tip=\"") + kWindowLayout.OptionsOf(WindowList::ShowCondition)[static_cast<size_t>(conditions[0])].tip + "\"").c_str()));
    CHECK(Has(checked.popup->rml, (std::string("id=\"p-c-show-o-0\" tip=\"") + kWindowLayout.WordOf(WindowWord::Always).tip + "\"").c_str()));
    // Under Always every condition reads checked.
    const Form always = pass({{"p-c-show-o-0", kClickEvent, ""}});
    CHECK(show.always && ClassOn(always, "p-c-show-o-0", "on") && ClassOn(always, "p-c-show-o-2", "on"));
    CHECK_EQ(TextOf(always, "p-c-show-val"), std::string(kWindowLayout.WordOf(WindowWord::Always).name));
    // Unchecking one of them turns Always off and leaves the others checked.
    const Form one = pass({{"p-c-show-o-2", kClickEvent, ""}});
    CHECK(one.changed && !show.always && one.popup);
    CHECK(!ClassOn(one, "p-c-show-o-0", "on") && !ClassOn(one, "p-c-show-o-2", "on") && ClassOn(one, "p-c-show-o-3", "on"));
    CHECK_EQ(TextOf(one, "p-c-show-val"), std::to_string(conditions.size() - 1) + " Conditions");
}

namespace
{
    // A card with a design line's box and one tag chip under it.
    struct BoxCard
    {
        FormState state;
        std::string text;
        LineDraft draft;
        bool edited = false, chip = false;
        std::string focused; // the control with the keyboard

        Form Pass(const std::vector<FormEvent>& events)
        {
            Form form(events, state);
            form.focused = focused;
            form.pageKey = "p";
            form.BeginCard("C");
            edited = form.LineBox("line", text, draft);
            chip   = form.TagChip(Tag::Con);
            form.EndCard();
            return form;
        }
    };
    const std::string kConChip = "tag-" + std::to_string(static_cast<int>(Tag::Con));
    // A chip let go on a part of a line box, with the slot it carries as a line's text holds it.
    FormEvent Drop(const char* on, const std::string& chip, const char* carried) { return FormEvent{on, kDropEvent, chip, carried}; }
    FormEvent LetGo(const char* chip) { return FormEvent{chip, kDragEndEvent, "", ""}; }

    // A card with two lines' boxes.
    struct TwoBoxes
    {
        FormState state;
        std::string top, bottom;
        LineDraft topDraft, bottomDraft;

        Form Pass(const std::vector<FormEvent>& events)
        {
            Form form(events, state);
            form.pageKey = "p";
            form.BeginCard("C");
            form.LineBox("top", top, topDraft);
            form.LineBox("bottom", bottom, bottomDraft);
            form.EndCard();
            return form;
        }
    };
}

TEST(a_line_box_is_its_stretches_of_text_with_its_tags_as_chips_between_them)
{
    BoxCard card;
    card.text = "Lv $level end";
    const Form first = card.Pass({});
    const std::string& rml = first.blocks[0].rml;
    // A stretch to type in either side of the tag, and the tag as the chip dragged in, with no mark before its name.
    const size_t before = rml.find("<input type=\"text\" class=\"run\" id=\"p-c-line-t-0\"/>");
    const size_t chip   = rml.find("id=\"p-c-line-c-0\"");
    const size_t after  = rml.find("<input type=\"text\" class=\"run\" id=\"p-c-line-t-1\"/>");
    CHECK(before != std::string::npos && chip != std::string::npos && after != std::string::npos);
    CHECK(before < chip && chip < after);
    CHECK(Has(rml, ">Level</div>") && !Has(rml, ">$level<"));
    CHECK_EQ(ValueText(first, "p-c-line-t-0"), "Lv ");
    CHECK_EQ(ValueText(first, "p-c-line-t-1"), " end");
    CHECK(!first.changed && !card.edited);

    // Typed in and left, a stretch is the design's; its caret is kept for a chip clicked after.
    card.focused = "p-c-line-t-1";
    card.Pass({});
    CHECK(card.draft.typing);
    // The keyboard gone, however that came about, the box is typed in no more.
    card.focused = "p-c-other";
    const Form typed = card.Pass({{"p-c-line-t-1", kCaretEvent, "2"}, {"p-c-line-t-1", kCommitEvent, " ends"}});
    CHECK(typed.changed && card.edited && card.text == "Lv $level ends");
    CHECK((card.draft.caret == RunCaret{1, 2}) && !card.draft.typing);
    // A tag typed by its name becomes a chip of its own.
    const Form tagged = card.Pass({{"p-c-line-t-0", kCommitEvent, "Lv $name "}});
    CHECK_EQ(card.text, "Lv $name $level ends");
    CHECK(Has(tagged.blocks[0].rml, "id=\"p-c-line-c-1\"") && Has(tagged.blocks[0].rml, "id=\"p-c-line-t-2\""));
}

TEST(backspace_or_delete_beside_a_tag_takes_the_whole_tag_out)
{
    BoxCard card;
    card.text = "Lv $name $level ends";
    // Backspace at the start of the stretch after Level, with what is typed there kept: the stretches either side
    // join, and the keyboard goes to where they met.
    const Form back = card.Pass({{"p-c-line-t-2", kKillEvent, std::string(1, kKillBack) + " ends!"}});
    CHECK(back.changed && card.edited);
    CHECK_EQ(card.text, "Lv $name  ends!");
    CHECK((back.focus == FormFocus{"p-c-line-t-1", 1}));
    CHECK((card.draft.caret == RunCaret{1, 1}));
    // Delete at the end of the stretch before Name.
    const Form forward = card.Pass({{"p-c-line-t-0", kKillEvent, std::string(1, kKillForward) + "Lv "}});
    CHECK_EQ(card.text, "Lv   ends!");
    CHECK((forward.focus == FormFocus{"p-c-line-t-0", 3}));
    // With no tag on that side, the key is the stretch's own: nothing is taken out and the keyboard stays.
    card.text = "$name end";
    const Form none = card.Pass({{"p-c-line-t-0", kKillEvent, std::string(1, kKillBack)}});
    CHECK(!none.changed && card.text == "$name end" && !none.focus);
    const Form last = card.Pass({{"p-c-line-t-1", kKillEvent, std::string(1, kKillForward) + " end"}});
    CHECK(!last.changed && card.text == "$name end" && !last.focus);
}

TEST(a_tag_chip_dropped_in_a_line_box_goes_after_what_it_is_dropped_on)
{
    BoxCard card;
    card.text = "Lv $level end";
    card.Pass({Drop("p-c-line", kConChip, "$con")}); // on the box itself: at its end
    CHECK_EQ(card.text, "Lv $level end$con");
    CHECK((card.draft.caret == RunCaret{2, 0})); // and a chip clicked next lands after it
    card.text = "Lv $level end";
    const Form onStretch = card.Pass({Drop("p-c-line-t-0", kConChip, "$con")}); // on a stretch: after it
    CHECK_EQ(card.text, "Lv $con$level end");
    CHECK(onStretch.changed && card.edited);
    // Let go inside a stretch, between the letters the pointer was nearest.
    card.text       = "Lv $level end";
    FormEvent split = Drop("p-c-line-t-0", kConChip, "$con");
    split.at        = 1;
    card.Pass({split});
    CHECK_EQ(card.text, "L${con}v $level end");
    CHECK((card.draft.caret == RunCaret{1, 0}));
    card.text = "Lv $con$level end";
    // Anything else dropped is nothing, and a click on the chip under the box is the page's to place.
    card.Pass({{"p-c-line", kDropEvent, "page-title"}});
    card.Pass({Drop("p-c-line", "page-title", "no tag")});
    CHECK_EQ(card.text, "Lv $con$level end");
    card.Pass({{kConChip, kClickEvent, ""}});
    CHECK(card.chip && card.text == "Lv $con$level end");
}

TEST(enter_in_a_stretch_breaks_the_line_there_and_typing_goes_on_after_the_break)
{
    BoxCard card;
    card.text = "Lv $level end";
    FormEvent enter{"p-c-line-t-0", kBreakEvent, "Lv ", {}, 2};
    const Form broken = card.Pass({enter});
    CHECK_EQ(card.text, "Lv$break $level end");
    CHECK(broken.changed && card.edited);
    CHECK((broken.focus == FormFocus{"p-c-line-t-1", 0}));
    CHECK((card.draft.caret == RunCaret{1, 0}));
    CHECK(Has(broken.blocks[0].rml, "class=\"btn accent chip in drag break\" id=\"p-c-line-c-0\""));
    // Nothing joins a break, and Backspace at the start of the row after it takes it out.
    card.Pass({Drop("p-c-line-c-0", kConChip, "$con")});
    CHECK_EQ(card.text, "Lv$break $level end");
    card.Pass({{"p-c-line-t-1", kKillEvent, std::string(1, kKillBack) + " "}});
    CHECK_EQ(card.text, "Lv $level end");
    // A line too long for one more tag keeps what was typed and breaks nowhere.
    card.text = std::string(kMaxTemplate - 5, 'a');
    card.Pass({{"p-c-line-t-0", kBreakEvent, card.text + "b", {}, 1}});
    CHECK_EQ(card.text, std::string(kMaxTemplate - 5, 'a') + "b");
}

TEST(a_tag_dropped_on_a_chip_shares_its_slot)
{
    BoxCard card;
    card.text = "Lv $level end";
    const Form joined = card.Pass({Drop("p-c-line-c-0", kConChip, "$con")});
    CHECK_EQ(card.text, "Lv ${level|con} end");
    CHECK(joined.changed && card.edited);
    // The one chip holds each as a part of its own, which is dragged alone and carries its tag wherever it goes.
    const std::string& rml = joined.blocks[0].rml;
    const size_t level = rml.find("id=\"p-c-line-c-0-0\" carries=\"$level\"");
    const size_t con   = rml.find("id=\"p-c-line-c-0-1\" carries=\"$con\"");
    CHECK(level != std::string::npos && con != std::string::npos && level < con);
    const Form again = card.Pass({Drop("p-c-line-c-0", kConChip, "$con")}); // it holds that tag already
    CHECK(!again.changed && card.text == "Lv ${level|con} end");
    // Backspace beside the chip takes the last of its tags, and the keyboard stays where it was.
    const Form back = card.Pass({{"p-c-line-t-1", kKillEvent, std::string(1, kKillBack) + " end"}});
    CHECK_EQ(card.text, "Lv $level end");
    CHECK((back.focus == FormFocus{"p-c-line-t-1", 0}));
    card.text = "Lv ${level|con} end";
    const Form forward = card.Pass({{"p-c-line-t-0", kKillEvent, std::string(1, kKillForward) + "Lv "}});
    CHECK_EQ(card.text, "Lv $level end");
    CHECK((forward.focus == FormFocus{"p-c-line-t-0", 3}));
}

TEST(a_chip_dragged_out_of_its_line_is_gone_and_one_let_go_in_the_line_has_moved)
{
    BoxCard card;
    card.text = "a $name b $level c";
    const Form out = card.Pass({LetGo("p-c-line-c-0")}); // let go on nothing that is a line
    CHECK_EQ(card.text, "a  b $level c");
    CHECK(out.changed && card.edited);
    // One tag of a chip that holds several goes alone.
    card.text = "a ${name|title} b $level c";
    card.Pass({LetGo("p-c-line-c-0-1")});
    CHECK_EQ(card.text, "a $name b $level c");
    card.text = "a ${name|title} b $level c";
    card.Pass({LetGo("p-c-line-c-0-0")});
    CHECK_EQ(card.text, "a $title b $level c");
    // Let go on another tag of its own chip, it takes that tag's place: the first of a chip's tags is the first tried.
    card.text = "a ${name|title} b $level c";
    const Form turned = card.Pass({Drop("p-c-line-c-0-0", "p-c-line-c-0-1", "$title"), LetGo("p-c-line-c-0-1")});
    CHECK_EQ(card.text, "a ${title|name} b $level c");
    CHECK(turned.changed);
    // On its own chip and no tag of it, it stays as it was.
    card.Pass({Drop("p-c-line-c-0", "p-c-line-c-0-1", "$name"), LetGo("p-c-line-c-0-1")});
    CHECK_EQ(card.text, "a ${title|name} b $level c");
    // One tag of a chip can go to a stretch, or to another chip, and the rest stay.
    card.text = "a ${name|title} b $level c";
    card.Pass({Drop("p-c-line-t-2", "p-c-line-c-0-1", "$title"), LetGo("p-c-line-c-0-1")});
    CHECK_EQ(card.text, "a $name b $level c$title");
    card.text = "a ${name|title} b $level c";
    card.Pass({Drop("p-c-line-c-1", "p-c-line-c-0-0", "$name"), LetGo("p-c-line-c-0-0")});
    CHECK_EQ(card.text, "a $title b ${level|name} c");
    card.text = "a ${name|title} b ${level|con} c";
    card.Pass({Drop("p-c-line-c-1-0", "p-c-line-c-0-1", "$title"), LetGo("p-c-line-c-0-1")}); // on a tag of another chip
    CHECK_EQ(card.text, "a $name b ${level|con|title} c");
    // Let go on a stretch of its own line, it goes after that stretch: from before it, and from after it.
    card.text = "a $name b $level c";
    card.Pass({Drop("p-c-line-t-2", "p-c-line-c-0", "$name"), LetGo("p-c-line-c-0")});
    CHECK_EQ(card.text, "a  b $level c$name");
    card.text = "a $name b $level c";
    card.Pass({Drop("p-c-line-t-0", "p-c-line-c-1", "$level"), LetGo("p-c-line-c-1")});
    CHECK_EQ(card.text, "a $level$name b  c");
    // On another chip of the line, it shares that chip's slot and leaves its own place.
    card.text = "a $name b $level c";
    card.Pass({Drop("p-c-line-c-1", "p-c-line-c-0", "$name"), LetGo("p-c-line-c-0")});
    CHECK_EQ(card.text, "a  b ${level|name} c");
    // On a chip that holds its tag already nothing joins, so it stays where it was.
    card.text = "$name ${level|name}";
    const Form same = card.Pass({Drop("p-c-line-c-1", "p-c-line-c-0", "$name"), LetGo("p-c-line-c-0")});
    CHECK(!same.changed && card.text == "$name ${level|name}");
}

TEST(a_chip_dragged_from_one_line_to_another_leaves_the_first)
{
    TwoBoxes card;
    card.top    = "$sync $name";
    card.bottom = "Lv $level";
    card.Pass({Drop("p-c-bottom", "p-c-top-c-0", "$sync"), LetGo("p-c-top-c-0")});
    CHECK_EQ(card.top, " $name");
    CHECK_EQ(card.bottom, "Lv $level$sync");
    // One tag of a chip of several goes alone, and on a chip of the other line it joins that chip.
    card.top    = "${sync|bazaar} $name";
    card.bottom = "Lv $level";
    card.Pass({Drop("p-c-bottom-c-0", "p-c-top-c-0-1", "$bazaar"), LetGo("p-c-top-c-0-1")});
    CHECK_EQ(card.top, "$sync $name");
    CHECK_EQ(card.bottom, "Lv ${level|bazaar}");
}

