#pragma once

#include "bar_document.h"
#include "color.h"
#include "markup.h"
#include "menu_sizes.h"
#include "menu_state.h"
#include "plate_settings.h"
#include "plate_template.h"
#include "window_layout.h"

#include <span>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace headsup
{
    // printf's format of these values as text.
    std::string Formatted(const char* format, auto... values)
    {
        char text[96];
        std::snprintf(text, sizeof(text), format, values...);
        return text;
    }

    // Something the player did to a control since the last pass, by the control's id.
    struct FormEvent
    {
        std::string id;
        std::string type;
        std::string value; // a change's or commit's new value, a drop's dragged control, a caret's place in bytes
        std::string carried = {}; // a drop's: what the dragged control carries (kCarriesAttribute), a tag chip's slot
    };
    constexpr const char* kClickEvent  = "click";
    constexpr const char* kChangeEvent = "change"; // a slider moved, a text box typed in
    constexpr const char* kCommitEvent = "commit"; // a text box left by Enter or a click elsewhere, its text changed
    constexpr const char* kCaretEvent  = "caret"; // a text box's caret as it loses the keyboard
    constexpr const char* kDropEvent   = "drop";
    // After the open control's id: a click on its popup that is on none of the popup's parts, which is neither a
    // click elsewhere, closing it, nor one on the control, which would.
    constexpr const char* kPopupGround = "-popup";
    // A dragged control was let go, on something or on nothing, anywhere but where it stands.
    constexpr const char* kDragEndEvent = "dragend";
    // The attribute of a control that can be dragged which says what a drop of it brings: a tag chip's slot, as a
    // line's text holds it.
    constexpr const char* kCarriesAttribute = "carries";
    // A tag beside a stretch of a line box is to go: Backspace at the stretch's start, or Delete at its end. Its
    // value is which (kKillBack or kKillForward) and then the stretch as it is typed.
    constexpr const char* kKillEvent   = "kill";
    constexpr char kKillBack = 'b', kKillForward = 'f';
    // The ids of a line box's parts, after the box's own: a stretch of text and a tag, each with its number.
    // Classes the kit writes, the window sets and the stylesheet draws: one name each for the three.
    constexpr const char* kOffClass     = "off";     // a setting of something switched off: not shown
    constexpr const char* kCompactCheck = "check";       // a toggle among other controls
    constexpr const char* kWideCheck    = "check wide";  // a row's toggle, as wide as a slider
    constexpr const char* kCardCheck    = "check card-on"; // a card title's switch
    constexpr const char* kPickedClass  = "picked";  // the chosen one of a row of choices
    constexpr const char* kLineBoxClass = "linebox"; // a design line's box
    constexpr const char* kRunClass     = "run";     // a stretch of text in it
    constexpr const char* kTypingClass  = "typing";  // on a box while one of its stretches has the keyboard
    constexpr const char* kRunIdInfix  = "-t-";
    // The ids of a control's parts, after the control's own: a dropdown's option, a row of choices' choice and a tab
    // strip's tab, each with its number.
    constexpr const char* kOptionIdInfix = "-o-", *kSegmentIdInfix = "-s-", *kTabIdInfix = "-t-";
    constexpr const char* kChipIdInfix = "-c-";
    constexpr const char* kChipTagInfix = "-"; // after a chip's id and before a tag's number, in a chip of several

    // What an element shows that is no part of the markup: set on the element when it changes, so a slider being
    // dragged or a row fading replaces nothing.
    enum class FormValueKind : uint8_t
    {
        Value,    // a slider's or a text box's value
        Text,     // an element's text
        Markup,   // an element's content, as RML
        Class,    // a class (name) on or off
        Property, // a style property (name)
        Attribute, // an attribute (name), gone while its text is empty
    };
    struct FormValue
    {
        std::string id;
        FormValueKind kind = FormValueKind::Text;
        std::string name, text;
        bool on = false;
        bool operator==(const FormValue&) const = default;
    };

    // An element of the window by its id and the markup inside it: the sidebar, and each card of the page.
    struct FormBlock
    {
        std::string id, rml;
        bool bare = false; // on the page itself under the cards, with no card around it
        bool operator==(const FormBlock&) const = default;
    };
    constexpr const char* kSidebarBlock = "sidebar";

    // A text box to give the keyboard, with its caret at this byte: where a tag was just taken out beside it.
    struct FormFocus
    {
        std::string id;
        size_t caret = 0;
        bool operator==(const FormFocus&) const = default;
    };

    // The one open popup, shown under the control it belongs to: a dropdown's list, a color's sliders.
    struct FormPopup
    {
        std::string anchor, rml;
        bool operator==(const FormPopup&) const = default;
    };

    // What the window keeps between passes that is no setting.
    struct FormState
    {
        std::unordered_set<std::string> collapsed; // the ids of the collapsed cards
        std::string popup;                         // the control whose popup is open, or none
    };

    // What an option of a dropdown does, by its place in the list: its tooltip. None says nothing.
    using OptionTip = std::function<std::string(int)>;

    // The classes a button takes for its look.
    constexpr const char* kRaisedPill = "btn raised";
    constexpr const char* kFlatPill   = "btn flat";
    constexpr const char* kAccentPill = "btn accent";
    constexpr const char* kWashPill   = "btn wash"; // a slider's filled part

    // The settings window's kit. A pass runs the page's code against it, as an immediate-mode window is drawn:
    // each call takes what the player did to its control since the last pass from the events, changes what it is
    // bound to, and writes the control's markup and what it shows now. The window then replaces only the blocks
    // whose markup changed and sets the values that did.
    class Form
    {
    public:
        // keyDown: whether a key is down now, by its virtual-key code; for the pages' key rows.
        Form(std::vector<FormEvent> events, FormState& state, std::function<bool(uint8_t)> keyDown = {});

        bool changed = false;     // a setting changed: its owner saves
        bool scrollToTop = false; // the page should go back to its top
        std::string pageKey;      // the page being built, which keeps two pages' cards of one title apart
        std::vector<FormBlock> blocks;
        std::vector<FormValue> values;
        std::optional<FormPopup> popup;
        std::optional<FormFocus> focus;
        std::function<bool(uint8_t)> keyDown;
        std::string focused; // the id of the control that has the keyboard, or none
        const WindowLayout* layout = &kWindowLayout; // the words of the kit's own lists

        // What follows is not shown and takes no clicks, while the feature it belongs to is off: a setting that
        // would do nothing is no setting. A card begun so is left out whole.
        void Fade(bool off) { m_Off = off; }
        // True in the pass a click opened the list of the dropdown of this name, the card's last so named.
        bool Opened(std::string_view name) const;
        // How many cards of this pass show, folded or not.
        int CardsShown() const { return m_CardsShown; }
        // The rows that follow stand in from the left: they belong to the row above them.
        void Indent(bool in) { m_Indent = in; }

        // Any element by id, to write markup into with Raw: the sidebar.
        void BeginBlock(const char* id);
        // A card, its title row folding it; an enabled checkbox sits at the title's right, and what the card is for
        // is written under the title. False when it is folded, with nothing more to do; true, with EndCard to call.
        bool BeginCard(const char* title, bool* enabled = nullptr, const char* about = nullptr);
        // A block with the id a card of that title would have and no title or card of its own; EndCard ends it.
        void BeginBare(const char* title);
        void EndCard();

        // A row: its label at the left, with its tip, and the controls that follow at the right, until EndRow.
        // In a label, what is before "##" shows and what follows it names the control's id, which is otherwise
        // named by the whole label.
        void Row(const char* label, const char* tip);
        void EndRow();
        // A line of controls across the card, with no label.
        void Line(const char* classes = "line");
        void EndLine();

        // A yes or no, in the middle of the controls' column. offWord and onWord, when given, say beside it which
        // of the two it is at.
        void Switch(const char* label, bool& on, const char* tip, const char* offWord = nullptr,
            const char* onWord = nullptr);
        // Options as joined buttons, the picked one filled.
        template <typename Enum>
        void Segments(const char* label, Enum& value, const char* const* options, int count, const char* tip,
            float segment = 0.0f)
        {
            Row(label, tip);
            int picked = static_cast<int>(value);
            SegmentsAt(label, picked, options, count, segment);
            value = static_cast<Enum>(picked);
            EndRow();
        }
        void SliderInt(const char* label, int& value, int lo, int hi, const char* format, const char* tip);
        void SliderPercent(const char* label, int& value, int lo, int hi, const char* tip);
        // A number stepped one at a time by the arrows either side of it, for one whose change moves the window under
        // the pointer, as a slider's would.
        void Stepper(const char* label, int& value, int lo, int hi, const char* format, const char* tip);
        void SliderFloat(const char* label, float& value, float lo, float hi, const char* format, const char* tip);
        // A slider over a few named steps, the first being 0: it shows the name of the one it is on.
        void SliderNamed(const char* label, int& value, const char* const* names, int count, const char* tip);
        // A dropdown of the whole numbers from first to last, either way round, each shown as text gives it.
        // option: what each number does, by the number.
        template <typename Name>
        void NumberDropdown(const char* label, int& value, int first, int last, Name text, const char* tip,
            float width = 0.0f, const OptionTip& option = {})
        {
            Row(label, tip);
            DropdownAt(label, value, first, last, text, width, nullptr, option);
            EndRow();
        }
        // A dropdown whose rows, and the closed control, show RML of the page's own where the others show words.
        void MarkupDropdown(const char* label, int& value, const std::vector<std::string>& rows, const char* tip,
            const OptionTip& option = {});
        // A dropdown of fonts between two arrows that step to the font before and after. unset, when given, is a
        // first choice that stands for an empty name.
        void FontDropdown(const char* label, std::string& font, const std::vector<std::string>& choices,
            const char* tip = nullptr, const char* unset = nullptr);
        // Joined buttons that each switch a bit of their own, the lowest first: any of them together.
        void Toggles(const char* label, uint8_t& bits, const char* const* options, int count, const char* tip,
            float segment = 0.0f);
        // original: the color's default. While the color is another, an arrow before the swatch puts it back.
        void Swatch(const char* label, Color& color, const char* tip, const Color* original = nullptr);
        // A color pick: the dropdown of its source, then the swatch of a custom color.
        void ColorPickRow(const char* label, ColorPick& pick, const std::vector<ColorSource>& sources, const char* tip);
        // When a line shows, as a dropdown of checks: Always, under which every condition reads checked, then each
        // condition.
        void ChecksDropdown(const char* label, LineShow& show, std::span<const ShowCondition> conditions,
            const char* tip);
        // An X and a Y to type, side by side.
        void Offset(const char* label, int& x, int& y, int limit, const char* tip);
        // A strip of equal tabs across the card, perRow to a row when given. Returns the tab clicked, or picked.
        int Tabs(const char* id, int picked, const char* const* labels, int count, int perRow = 0);
        // A muted line inside the card, wrapped at its edge.
        void Note(const char* text);
        // A pill action on a line of its own.
        bool Action(const char* label, const char* tip = nullptr);
        // A label and a text that changes without the row changing: a reading, not a setting.
        void Reading(const char* label, const std::string& text);

        // Controls for inside a row or a line. Each id is the control's own within its card.
        void CheckAt(const char* id, bool& on, const char* tip = nullptr);
        void SegmentsAt(const char* id, int& value, const char* const* options, int count, float segment = 0.0f);
        // A dropdown whose list is a popup of rows, the picked one marked. True when another row was chosen. The
        // closed control's tip is tip, or what option says of the row picked; markup rows are RML, not text.
        template <typename Name>
        bool ListAt(const char* id, const std::string& shown, float width, int count, Name name, int picked, int& chosen,
            const char* tip = nullptr, const OptionTip& option = {}, bool markup = false)
        {
            const std::string control = Id(id);
            const bool listed         = picked >= 0 && picked < count;
            const bool open = ListControl(control, shown, width,
                tip != nullptr ? tip : option && listed ? option(picked) : std::string(), markup);
            if (!open) return false;
            std::string rml;
            for (int i = 0; i < count; ++i)
                rml += ListRow(control, i, name(i), i == picked, option ? option(i) : std::string(), markup);
            return ListChoice(control, std::move(rml), count, chosen) && chosen != picked;
        }
        template <typename Name>
        void DropdownAt(const char* id, int& value, int first, int last, Name text, float width = 0.0f,
            const char* tip = nullptr, const OptionTip& option = {})
        {
            const int step  = first <= last ? 1 : -1;
            const int count = (last - first) * step + 1;
            int chosen      = 0;
            const OptionTip byNumber = option ? OptionTip([&](int i) { return option(first + i * step); }) : OptionTip();
            if (ListAt(id, text(value), width, count, [&](int i) { return text(first + i * step); },
                    (value - first) * step, chosen, tip, byNumber))
            {
                value   = first + chosen * step;
                changed = true;
            }
        }
        // A whole number to type, with its name before it: "X [ 12 ]".
        void NumberAt(const char* name, const char* id, int& value, int limit);
        void SwatchAt(const char* id, Color& color);
        // The arrow that puts a value back to its default, hidden while it is the default. True when it was clicked.
        bool ResetAt(const char* id, bool differs);
        // A pill. classes: kRaisedPill and the like, and any more for the page's own looks. True when clicked.
        bool Button(const char* label, const char* id, const char* classes = kRaisedPill, const char* tip = nullptr);
        // A text box. Committed: true when it was left by Enter or a click elsewhere with its text changed, which
        // text then holds. Live: true whenever a key changed it.
        bool TextAt(const char* id, std::string& text, const char* hint, float width, bool live = false,
            size_t most = 0);
        // A design line's box: stretches of text to type in with its slots as chips between them, which take drops
        // and can be dragged. True when its text changed. draft keeps its caret, and says whether it has the keyboard.
        bool LineBox(const char* id, std::string& text, LineDraft& draft);
        // A tag as a chip that reads its name, after its icon when it draws one. True when clicked; dragged, it drops
        // onto a line's box.
        bool TagChip(Tag tag);
        // Muted or plain text among a row's controls.
        void Caption(const char* text, const char* classes = "caption", float width = 0.0f);
        // Markup of the page's own, for what the kit has no control for; and a value for an element of it.
        void Raw(const std::string& rml) { Out() += rml; }
        void Set(std::string id, FormValueKind kind, std::string name, std::string text, bool on = false);

        // A control's id from its name in the card being built: the same every pass, and no other control's.
        std::string Id(std::string_view name);
        // The id the first control of this name in the card gets, without taking it.
        std::string Named(std::string_view name) const;
        // What the player did to a control since the last pass: the newest event of the kind, or nothing.
        const FormEvent* Took(std::string_view id, const char* type) const;
        bool Clicked(std::string_view id) const { return Took(id, kClickEvent) != nullptr; }
        bool Off() const { return m_Off; }

    private:
        std::string& Out() { return blocks.back().rml; }
        // The label a row shows: what is before "##", as the layout words it.
        static std::string Shown(const char* label);
        // A dropdown's closed control: what it shows and its arrow. True while its popup is open.
        // markup: shown, or name, is RML and not words to escape.
        // A row of joined buttons: press is told of each one clicked, and picked says which are lit.
        void SegmentRow(const std::string& base, const char* const* options, int count, float segment,
            const std::function<void(int)>& press, const std::function<bool(int)>& picked);
        // A click on the control opens its popup, or closes it when it is the one open.
        void TogglePopup(const std::string& control);
        bool ListControl(const std::string& control, const std::string& shown, float width, const std::string& tip = {},
            bool markup = false);
        std::string ListRow(const std::string& control, int index, const std::string& name, bool picked,
            const std::string& tip = {}, bool markup = false);
        // Opens the popup with rows, and tells which of them was clicked.
        bool ListChoice(const std::string& control, std::string rows, int count, int& chosen);
        // A slider's newest whole value, when it moved.
        void TakeWhole(const std::string& slider, int& value, int lo, int hi, int step = 1);
        void Slider(const std::string& slider, const char* label, float value, float lo, float hi, float step,
            const std::string& shown, const char* tip);
        // A toggle, and its id: a row's is wide, one among other controls compact.
        // With words, the toggle says its state in its middle.
        std::string Check(const char* id, bool& on, const char* tip, const char* classes = kCompactCheck,
            const char* offWord = nullptr, const char* onWord = nullptr);
        // A button of any markup inside. True when clicked.
        bool Pressable(const std::string& inside, const char* id, const char* classes, const char* tip);
        // The left (previous) or right (next) arrow beside a control, its id the label's with that word.
        bool StepArrow(const std::string& label, bool next);

        std::vector<FormEvent> m_Events;
        FormState& m_State;
        bool m_Off       = false;
        bool m_Indent    = false;
        int m_CardsShown = 0;
        size_t m_CardOff = 0;         // the card's own off value, in values
        bool m_CardSwitched = false;  // whether the card has a switch in its title
        int m_ShownInCard = 0;        // its parts that show
        std::string m_CardId;
        std::unordered_set<std::string> m_Ids; // this pass's, to keep them apart
    };
}
