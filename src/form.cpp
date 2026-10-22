#include "form.h"

#include "menu_document.h"

#include "argb.h"
#include "bar_document.h"
#include "menu_text.h"
#include "settings.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <utility>

namespace headsup
{
    namespace
    {
        constexpr std::string_view kIdMark = "##"; // in a label: before it what shows, after it what names the id
        constexpr float kColorFull        = 255.0f;
        constexpr int kHexDigits          = 6;
        constexpr int kNumberDigits       = 6; // a number box holds a sign and five digits
        // The room a square button takes beside a dropdown: the swatch, and each of two step arrows.
        constexpr float kSquareRoom = kFrame + kControlGap;
        // Between two step arrows (the font's, the window's size), so the row ends where a slider's does.
        constexpr float kBetweenArrows = kControlWidth - 2.0f * kSquareRoom;
        constexpr int kHexBase            = 16;
        constexpr const char* kNoteName      = "note";     // a note's id, as its card's controls are named
        constexpr const char* kCaptionSuffix = " caption"; // a caption's, after its text
        constexpr const char* kChannels[] = {"r", "g", "b", "a"};
        constexpr const char* kChannelNames[] = {"R", "G", "B", "A"};
        constexpr size_t kRgbChannels   = 3;
        constexpr uint32_t kRgbMoved    = 0b111; // the hex box sets the three
        constexpr int kChannelBits      = 8;
        constexpr const char* kTagIdStart = "tag-"; // a tag chip's id, the same on every page

        // An id's characters: letters and figures in lower case, anything else a hyphen, never two in a row.
        std::string Slug(std::string_view text)
        {
            std::string out;
            for (const char c : text)
            {
                const auto u = static_cast<unsigned char>(c);
                if (std::isalnum(u) != 0)
                    out += static_cast<char>(std::tolower(u));
                else if (!out.empty() && out.back() != '-')
                    out += '-';
            }
            while (!out.empty() && out.back() == '-')
                out.pop_back();
            return out;
        }

        // What a label names its control's id by, and what it shows.
        std::string_view IdName(std::string_view label)
        {
            const size_t mark = label.find(kIdMark);
            return mark == std::string_view::npos ? label : label.substr(mark + kIdMark.size());
        }
        std::string_view LabelText(std::string_view label) { return label.substr(0, label.find(kIdMark)); }

        std::string TipAttribute(std::string_view tip)
        {
            return tip.empty() ? std::string() : std::string(" ") + kMenuTipAttribute + "=\"" + EscapedRml(tip) + "\"";
        }
        std::string TipAttribute(const char* tip) { return TipAttribute(std::string_view(tip != nullptr ? tip : "")); }

        // The step a float slider moves by: one of the last decimal its format shows.
        float StepOf(const char* format)
        {
            const char* point = std::strstr(format, "%.");
            const int decimals = point != nullptr ? std::atoi(point + 2) : 0;
            return std::pow(10.0f, static_cast<float>(-decimals));
        }

        uint32_t RgbOf(const Color& color) { return ToArgb(color) & ~kAlphaMask; }
    }

    Form::Form(std::vector<FormEvent> events, FormState& state, std::function<bool(uint8_t)> down)
        : keyDown(std::move(down)), m_Events(std::move(events)), m_State(state)
    {
        // A click anywhere but on the open popup or its control closes it.
        if (m_State.popup.empty()) return;
        for (const FormEvent& event : m_Events)
            if (event.type == kClickEvent && event.id.compare(0, m_State.popup.size(), m_State.popup) != 0)
            {
                m_State.popup.clear();
                break;
            }
    }

    std::string Form::Id(std::string_view name)
    {
        std::string id = Named(name);
        if (!m_Ids.insert(id).second)
            for (int again = 2;; ++again)
                if (std::string next = id + "-" + std::to_string(again); m_Ids.insert(next).second)
                {
                    id = std::move(next);
                    break;
                }
        return id;
    }

    std::string Form::Named(std::string_view name) const
    {
        return m_CardId.empty() ? Slug(IdName(name)) : m_CardId + "-" + Slug(IdName(name));
    }

    const FormEvent* Form::Took(std::string_view id, const char* type) const
    {
        for (auto event = m_Events.rbegin(); event != m_Events.rend(); ++event)
            if (event->id == id && event->type == type) return &*event;
        return nullptr;
    }

    void Form::Set(std::string id, FormValueKind kind, std::string name, std::string text, bool on)
    {
        if (kind == FormValueKind::Class && !on && name == kOffClass && id != m_CardId) ++m_ShownInCard;
        values.push_back(FormValue{std::move(id), kind, std::move(name), std::move(text), on});
    }

    std::string Form::Shown(const char* label)
    {
        return std::string(LabelText(label));
    }

    void Form::BeginBlock(const char* id)
    {
        blocks.push_back(FormBlock{id, {}, false});
        m_CardId = id;
    }

    bool Form::BeginCard(const char* title, bool* enabled, const char* about)
    {
        m_CardId = Slug(pageKey + "/" + std::string(IdName(title)));
        blocks.push_back(FormBlock{m_CardId, {}, false});
        m_CardOff       = values.size();
        m_CardSwitched  = enabled != nullptr;
        m_ShownInCard   = 0;
        Set(m_CardId, FormValueKind::Class, kOffClass, {}, m_Off);
        if (!m_Off) ++m_CardsShown;
        const std::string head = m_CardId + "-head";
        if (!m_Off && Clicked(head) && !m_State.collapsed.erase(m_CardId)) m_State.collapsed.insert(m_CardId);
        const bool open = !m_Off && !m_State.collapsed.contains(m_CardId);
        Out() += "<div class=\"card-head\" id=\"" + head + "\"><div class=\"arrow" + (open ? " open" : "") +
                 "\"></div><div class=\"card-title\">" + EscapedRml(LabelText(title)) + "</div>";
        if (enabled != nullptr)
        {
            const std::string check = m_CardId + "-on";
            if (!m_Off && Clicked(check))
            {
                *enabled = !*enabled;
                changed  = true;
            }
            const char* word = layout->WordOf(*enabled ? WindowWord::SwitchOn : WindowWord::SwitchOff).name;
            Out() += std::string("<div class=\"") + kCardCheck + "\" id=\"" + check + "\"><div class=\"mark\"></div><div class=\"state\" id=\"" +
                     check + "-state\"></div></div>";
            Set(check + "-state", FormValueKind::Text, {}, word);
            Set(check, FormValueKind::Class, "on", {}, *enabled);
        }
        Out() += "</div>";
        if (!open) return false;
        Out() += "<div class=\"card-body\">";
        if (about != nullptr && *about != '\0') Out() += "<div class=\"note\">" + EscapedRml(about) + "</div>";
        return true;
    }

    void Form::BeginBare(const char* title)
    {
        m_CardId = Slug(pageKey + "/" + std::string(IdName(title)));
        blocks.push_back(FormBlock{m_CardId, {}, true});
        m_CardOff      = values.size();
        m_CardSwitched = false;
        m_ShownInCard  = 0;
        Set(m_CardId, FormValueKind::Class, kOffClass, {}, m_Off);
        if (!m_Off) ++m_CardsShown;
        Out() += "<div>";
    }

    void Form::EndCard()
    {
        Out() += "</div>";
        // Its settings may all be of what is switched off: a card that shows nothing goes, unless its own switch is
        // the way to switch them on.
        if (m_ShownInCard > 0 || m_CardSwitched || values[m_CardOff].on) return;
        values[m_CardOff].on = true;
        --m_CardsShown;
    }

    void Form::Row(const char* label, const char* tip)
    {
        const std::string id = Id(std::string(label) + " label");
        const std::string row = id + "-row";
        Out() += std::string("<div class=\"row") + (m_Indent ? " indent" : "") + "\" id=\"" + row + "\"><div class=\"label\" id=\"" +
                 id + "\"" + TipAttribute(tip) + ">" + EscapedRml(Shown(label)) + "</div><div class=\"ctl\">";
        Set(row, FormValueKind::Class, kOffClass, {}, m_Off);
    }

    void Form::EndRow() { Out() += "</div></div>"; }

    void Form::Line(const char* classes) { Out() += std::string("<div class=\"") + classes + "\">"; }
    void Form::EndLine() { Out() += "</div>"; }

    std::string Form::Check(const char* id, bool& on, const char* tip, const char* classes, const char* offWord,
        const char* onWord)
    {
        const std::string check = Id(id);
        if (!m_Off && Clicked(check))
        {
            on      = !on;
            changed = true;
        }
        Out() += "<div class=\"" + std::string(classes) + "\" id=\"" + check + "\"" + TipAttribute(tip) + "><div class=\"mark\"></div>";
        if (offWord != nullptr && onWord != nullptr)
        {
            Out() += "<div class=\"state\" id=\"" + check + "-state\"></div>";
            Set(check + "-state", FormValueKind::Text, {}, on ? onWord : offWord);
        }
        Out() += "</div>";
        Set(check, FormValueKind::Class, "on", {}, on);
        Set(check, FormValueKind::Class, kOffClass, {}, m_Off);
        return check;
    }

    void Form::CheckAt(const char* id, bool& on, const char* tip) { Check(id, on, tip); }

    void Form::ToggleAt(const char* id, bool& on, const char* offWord, const char* onWord)
    {
        Check(id, on, nullptr, kWordedCheck, offWord, onWord);
    }

    void Form::Switch(const char* label, bool& on, const char* tip, const char* offWord, const char* onWord)
    {
        Row(label, tip);
        Check(label, on, nullptr, kWideCheck, offWord != nullptr ? offWord : layout->WordOf(WindowWord::SwitchOff).name,
            onWord != nullptr ? onWord : layout->WordOf(WindowWord::SwitchOn).name);
        EndRow();
    }

    void Form::SegmentRow(const std::string& base, const char* const* options, int count, float segment,
        const std::function<void(int)>& press, const std::function<bool(int)>& picked)
    {
        const std::string width = Em(segment > 0.0f ? segment : kSegmentWidth);
        Out() += "<div class=\"segs\" id=\"" + base + "\">";
        for (int option = 0; option < count; ++option)
        {
            const std::string seg = base + kSegmentIdInfix + std::to_string(option);
            if (!m_Off && Clicked(seg)) press(option);
            Out() += "<div class=\"seg\" id=\"" + seg + "\" style=\"width:" + width + ";\">" +
                     EscapedRml(options[option]) + "</div>";
        }
        Out() += "</div>";
        for (int option = 0; option < count; ++option)
            Set(base + kSegmentIdInfix + std::to_string(option), FormValueKind::Class, kPickedClass, {}, picked(option));
        Set(base, FormValueKind::Class, kOffClass, {}, m_Off);
    }

    void Form::SegmentsAt(const char* id, int& value, const char* const* options, int count, float segment)
    {
        SegmentRow(Id(id), options, count, segment,
            [&](int option) {
                changed |= value != option;
                value = option;
            },
            [&](int option) { return value == option; });
    }

    void Form::Slider(const std::string& slider, const char* label, float value, float lo, float hi, float step,
        const std::string& shown, const char* tip)
    {
        Row(label, tip);
        Out() += "<div class=\"slider\" id=\"" + slider + "-box\"><input type=\"range\" id=\"" + slider + "\" min=\"" +
                 RcssNumber(lo) + "\" max=\"" + RcssNumber(hi) + "\" step=\"" + RcssNumber(step) +
                 "\"/><div class=\"val\" id=\"" + slider + "-val\"></div></div>";
        Set(slider, FormValueKind::Value, {}, RcssNumber(value));
        Set(slider + "-val", FormValueKind::Text, {}, shown);
        Set(slider + "-box", FormValueKind::Class, kOffClass, {}, m_Off);
        EndRow();
    }

    bool Form::Opened(std::string_view name) const
    {
        const std::string control = Named(name);
        return m_State.popup == control && Clicked(control);
    }

    void Form::TakeWhole(const std::string& slider, int& value, int lo, int hi, int step)
    {
        const FormEvent* moved = Took(slider, kChangeEvent);
        if (moved == nullptr || m_Off) return;
        const int to = std::clamp(Stepped(static_cast<int>(std::lround(std::strtof(moved->value.c_str(), nullptr))), step), lo, hi);
        if (to == value) return;
        value   = to;
        changed = true;
    }

    void Form::SliderInt(const char* label, int& value, int lo, int hi, const char* format, const char* tip)
    {
        const std::string slider = Id(label);
        TakeWhole(slider, value, lo, hi);
        Slider(slider, label, static_cast<float>(value), static_cast<float>(lo), static_cast<float>(hi), 1.0f,
            Formatted(format, value), tip);
    }

    void Form::SliderPercent(const char* label, int& value, int lo, int hi, const char* tip)
    {
        const std::string slider = Id(label);
        TakeWhole(slider, value, lo, hi, kPercentStep);
        Slider(slider, label, static_cast<float>(value), static_cast<float>(lo), static_cast<float>(hi),
            static_cast<float>(kPercentStep), Formatted("%d%%", value), tip);
    }

    void Form::Stepper(const char* label, int& value, int lo, int hi, const char* format, const char* tip)
    {
        Row(label, tip);
        const std::string name  = label;
        const std::string shown = Id(label) + "-val";
        int stepped             = value;
        if (StepArrow(name, false)) --stepped;
        Out() += "<div class=\"stepval\" id=\"" + shown + "\" style=\"width:" + Em(kBetweenArrows) + ";\"></div>";
        if (StepArrow(name, true)) ++stepped;
        stepped = std::clamp(stepped, lo, hi);
        if (stepped != value)
        {
            value   = stepped;
            changed = true;
        }
        Set(shown, FormValueKind::Text, {}, Formatted(format, value));
        EndRow();
    }

    void Form::SliderNamed(const char* label, int& value, const char* const* names, int count, const char* tip)
    {
        const std::string slider = Id(label);
        TakeWhole(slider, value, 0, count - 1);
        Slider(slider, label, static_cast<float>(value), 0.0f, static_cast<float>(count - 1), 1.0f,
            names[std::clamp(value, 0, count - 1)], tip);
    }

    void Form::SliderFloat(const char* label, float& value, float lo, float hi, const char* format, const char* tip)
    {
        const std::string slider = Id(label);
        if (const FormEvent* moved = Took(slider, kChangeEvent); moved != nullptr && !m_Off)
        {
            const float to = std::clamp(std::strtof(moved->value.c_str(), nullptr), lo, hi);
            if (to != value)
            {
                value   = to;
                changed = true;
            }
        }
        Slider(slider, label, value, lo, hi, StepOf(format), Formatted(format, static_cast<double>(value)), tip);
    }

    void Form::TogglePopup(const std::string& control)
    {
        if (m_Off || !Clicked(control)) return;
        if (m_State.popup == control)
            m_State.popup.clear();
        else
            m_State.popup = control;
    }

    bool Form::ListControl(const std::string& control, const std::string& shown, float width, const std::string& tip, bool markup)
    {
        TogglePopup(control);
        const bool open = m_State.popup == control;
        // What the closed control says of its pick is set as a value: picking another builds nothing anew.
        Out() += "<div class=\"dd\" id=\"" + control + "\" style=\"width:" + Em(width > 0.0f ? width : kControlWidth) +
                 ";\"><div class=\"dd-val\" id=\"" + control + "-val\"></div><div class=\"dd-arrow\"></div></div>";
        Set(control, FormValueKind::Attribute, kMenuTipAttribute, tip);
        Set(control + "-val", markup ? FormValueKind::Markup : FormValueKind::Text, {}, shown);
        Set(control, FormValueKind::Class, kOffClass, {}, m_Off);
        Set(control, FormValueKind::Class, "open", {}, open);
        return open;
    }

    void Form::MarkupDropdown(const char* label, int& value, const std::vector<std::string>& rows, const char* tip,
        const OptionTip& option)
    {
        Row(label, tip);
        const int count  = static_cast<int>(rows.size());
        const int picked = std::clamp(value, 0, count - 1);
        int chosen       = picked;
        if (ListAt(label, rows[static_cast<size_t>(picked)], 0.0f, count, [&](int i) { return rows[static_cast<size_t>(i)]; },
                picked, chosen, nullptr, option, true))
        {
            value   = chosen;
            changed = true;
        }
        EndRow();
    }

    std::string Form::ListRow(const std::string& control, int index, const std::string& name, bool picked,
        const std::string& tip, bool markup)
    {
        const std::string row = control + kOptionIdInfix + std::to_string(index);
        Set(row, FormValueKind::Class, kPickedClass, {}, picked);
        return "<div class=\"opt\" id=\"" + row + "\"" + TipAttribute(tip) + ">" + (markup ? name : EscapedRml(name)) + "</div>";
    }

    bool Form::ListChoice(const std::string& control, std::string rows, int count, int& chosen)
    {
        const std::string start = control + kOptionIdInfix;
        for (auto event = m_Events.rbegin(); event != m_Events.rend(); ++event)
        {
            if (event->type != kClickEvent || event->id.compare(0, start.size(), start) != 0) continue;
            const int index = std::atoi(event->id.c_str() + start.size());
            if (index < 0 || index >= count) continue;
            chosen = index;
            m_State.popup.clear();
            return true;
        }
        popup = FormPopup{control, "<div class=\"list\">" + std::move(rows) + "</div>"};
        return false;
    }

    void Form::Toggles(const char* label, uint8_t& bits, const char* const* options, int count, const char* tip, float segment)
    {
        Row(label, tip);
        const auto bit = [](int option) { return static_cast<uint8_t>(1u << option); };
        SegmentRow(Id(label), options, count, segment,
            [&](int option) {
                bits ^= bit(option);
                changed = true;
            },
            [&](int option) { return (bits & bit(option)) != 0; });
        EndRow();
    }

    void Form::FontDropdown(const char* label, std::string& font, const std::vector<std::string>& choices, const char* tip,
        const char* unset)
    {
        Row(label, tip);
        const int count  = FontEntryCount(choices, unset);
        const int picked = FontEntry(choices, unset, font);
        int chosen       = picked;
        const std::string name = label;
        if (StepArrow(name, false)) chosen = StepFontEntry(count, picked, -1);
        int listed = 0;
        // A long family's name is cut in the list and in the closed control: both say it whole under the pointer.
        const std::string shown = FontShown(choices, unset, font);
        if (ListAt(label, shown, kBetweenArrows, count,
                [&](int entry) { return FontEntryLabel(choices, unset, entry); }, picked, listed, shown.c_str(),
                [&](int entry) { return FontEntryLabel(choices, unset, entry); }))
            chosen = listed;
        if (StepArrow(name, true)) chosen = StepFontEntry(count, picked, 1);
        if (chosen != picked && chosen != kNoFontEntry)
        {
            font    = FontOfEntry(choices, unset, chosen);
            changed = true;
        }
        EndRow();
    }

    uint32_t Form::Picker(const std::string& swatch, std::span<int> levels)
    {
        uint32_t moved = 0;
        for (size_t channel = 0; channel < levels.size(); ++channel)
        {
            // A slider says its value as it is made and each time it is set: only a level other than the color's own
            // is the player's, or opening the picker would round the color to it.
            const FormEvent* event = Took(swatch + "-p-" + kChannels[channel], kChangeEvent);
            if (event == nullptr) continue;
            const int to = static_cast<int>(std::lround(std::clamp(std::strtof(event->value.c_str(), nullptr), 0.0f, kColorFull)));
            if (to == levels[channel]) continue;
            levels[channel] = to;
            moved |= 1u << channel;
        }
        const std::string hex = swatch + "-p-hex";
        if (const FormEvent* typed = Took(hex, kCommitEvent))
        {
            const char* text = typed->value.c_str() + (typed->value.starts_with("#") ? 1 : 0);
            char* end        = nullptr;
            const unsigned long rgb = std::strtoul(text, &end, kHexBase);
            if (end == text + kHexDigits && *end == '\0')
            {
                for (size_t channel = 0; channel < kRgbChannels; ++channel)
                    levels[channel] = static_cast<int>((rgb >> (kChannelBits * (kRgbChannels - 1 - channel))) & 0xFF);
                moved |= kRgbMoved;
            }
        }
        // Its channels as sliders, and the color as it is written, to type.
        std::string rml = "<div class=\"picker\">";
        uint32_t rgb    = 0;
        for (size_t channel = 0; channel < levels.size(); ++channel)
        {
            const std::string slider = swatch + "-p-" + kChannels[channel];
            rml += std::string("<div class=\"pick-row\"><div class=\"pick-name\">") + kChannelNames[channel] +
                   "</div><input type=\"range\" id=\"" + slider + "\" min=\"0\" max=\"" + Formatted("%.0f", kColorFull) + "\" step=\"1\"/>" +
                   "<div class=\"val\" id=\"" + slider + "-val\"></div></div>";
            Set(slider, FormValueKind::Value, {}, std::to_string(levels[channel]));
            Set(slider + "-val", FormValueKind::Text, {}, std::to_string(levels[channel]));
            if (channel < kRgbChannels) rgb = rgb << kChannelBits | static_cast<uint32_t>(levels[channel]);
        }
        rml += "<div class=\"pick-row\"><div class=\"pick-name\">#</div><input type=\"text\" class=\"hex\" id=\"" + hex +
               "\" maxlength=\"" + std::to_string(kHexDigits + 1) + "\"/></div></div>"; // the digits after a #
        char written[16];
        std::snprintf(written, sizeof(written), "%0*X", kHexDigits, static_cast<unsigned>(rgb));
        Set(hex, FormValueKind::Value, {}, written);
        popup = FormPopup{swatch, std::move(rml)};
        return moved;
    }

    void Form::SwatchAt(const char* id, Color& color)
    {
        const std::string swatch = Id(id);
        TogglePopup(swatch);
        Out() += "<div class=\"swatch\" id=\"" + swatch + "\"></div>";
        if (m_State.popup == swatch)
        {
            std::array<int, kRgbChannels> levels{};
            for (size_t channel = 0; channel < kRgbChannels; ++channel)
                levels[channel] = static_cast<int>(std::lround(color.v[channel] * kColorFull));
            // Only the channels moved are set, so a color between two levels keeps the others as they were.
            const uint32_t moved = Picker(swatch, levels);
            for (size_t channel = 0; channel < kRgbChannels; ++channel)
                if ((moved >> channel & 1u) != 0) color.v[channel] = static_cast<float>(levels[channel]) / kColorFull;
            changed |= moved != 0;
        }
        Set(swatch, FormValueKind::Property, "background-color", RcssColor(Opaque(RgbOf(color))));
        Set(swatch, FormValueKind::Class, kOffClass, {}, m_Off);
    }

    void Form::ArgbSwatchAt(const char* id, uint32_t& argb)
    {
        const std::string swatch = Id(id);
        TogglePopup(swatch);
        Out() += "<div class=\"swatch\" id=\"" + swatch + "\"></div>";
        if (m_State.popup == swatch)
        {
            // In the picker's order, red, green, blue and alpha: the color's bytes from its second, then its first.
            constexpr int kShifts[] = {16, 8, 0, 24};
            std::array<int, std::size(kShifts)> levels{};
            for (size_t channel = 0; channel < levels.size(); ++channel)
                levels[channel] = static_cast<int>(argb >> kShifts[channel] & 0xFF);
            if (Picker(swatch, levels) != 0)
            {
                argb = 0;
                for (size_t channel = 0; channel < levels.size(); ++channel)
                    argb |= static_cast<uint32_t>(levels[channel]) << kShifts[channel];
                changed = true;
            }
        }
        Set(swatch, FormValueKind::Property, "background-color", RcssColor(argb));
        Set(swatch, FormValueKind::Class, kOffClass, {}, m_Off);
    }

    void Form::ArgbSwatch(const char* label, uint32_t& argb, const char* tip)
    {
        Row(label, tip);
        ArgbSwatchAt(label, argb);
        EndRow();
    }

    void Form::Swatch(const char* label, Color& color, const char* tip, const Color* original)
    {
        Row(label, tip);
        if (original != nullptr && ResetAt((std::string(label) + " reset").c_str(), color != *original))
        {
            color   = *original;
            changed = true;
        }
        SwatchAt(label, color);
        EndRow();
    }

    bool Form::ResetAt(const char* id, bool differs)
    {
        const std::string reset = Id(id);
        const bool clicked      = !m_Off && differs && Clicked(reset);
        // In the markup either way, hidden while there is nothing to put back: a value changed builds nothing.
        Out() += "<div class=\"reset\" id=\"" + reset + "\" tip=\"Back to its default.\"></div>";
        Set(reset, FormValueKind::Class, kHiddenClass, {}, !differs || clicked);
        return clicked;
    }

    void Form::ColorPickRow(const char* label, ColorPick& pick, const std::vector<ColorSource>& sources, const char* tip)
    {
        Row(label, tip);
        const auto at    = std::ranges::find(sources, pick.source);
        const auto words = layout->OptionsOf(WindowList::ColorSource);
        const auto said  = [&](ColorSource source) { return words[static_cast<size_t>(source)]; };
        int chosen       = 0;
        // The swatch takes its room from the dropdown, which keeps its place for any other source.
        if (ListAt(label, std::string(Said(said(pick.source).name)), kControlWidth - kSquareRoom, static_cast<int>(sources.size()),
                [&](int i) { return std::string(Said(said(sources[static_cast<size_t>(i)]).name)); },
                at != sources.end() ? static_cast<int>(at - sources.begin()) : -1, chosen, nullptr,
                [&](int i) { return std::string(Said(said(sources[static_cast<size_t>(i)]).tip)); }))
        {
            pick.source = sources[static_cast<size_t>(chosen)];
            changed     = true;
        }
        if (pick.source == ColorSource::Custom)
            SwatchAt((std::string(label) + " swatch").c_str(), pick.custom);
        else
            Out() += "<div class=\"swatch none\"></div>";
        EndRow();
    }

    void Form::ChecksDropdown(const char* label, LineShow& show, std::span<const ShowCondition> conditions,
        const char* tip)
    {
        Row(label, tip);
        const std::string control = Id(label);
        const std::string start   = control + kOptionIdInfix;
        // Before the control is written, so its summary is what the click just made it.
        if (m_State.popup == control)
            for (const FormEvent& event : m_Events)
            {
                if (event.type != kClickEvent || event.id.compare(0, start.size(), start) != 0) continue;
                changed |= ToggleCondition(show, conditions, std::atoi(event.id.c_str() + start.size()));
            }
        if (ListControl(control, ShowSummary(show, conditions, *layout), kControlWidth))
        {
            std::string rml = "<div class=\"list\">";
            const auto row  = [&](int index, const char* name, const char* tip, bool on) {
                const std::string id = start + std::to_string(index);
                rml += "<div class=\"opt chk\" id=\"" + id + "\"" + TipAttribute(tip) + "><div class=\"dot\"></div>" +
                       EscapedRml(name) + "</div>";
                Set(id, FormValueKind::Class, "on", {}, on);
            };
            const LayoutWord& always = layout->WordOf(WindowWord::Always);
            const auto names         = layout->OptionsOf(WindowList::ShowCondition);
            row(0, always.name, always.tip, show.always);
            for (size_t i = 0; i < conditions.size(); ++i)
            {
                const LayoutOption& condition = names[static_cast<size_t>(conditions[i])];
                row(static_cast<int>(i) + 1, condition.name, condition.tip, show.always || (show.when & ConditionBit(conditions[i])) != 0);
            }
            popup = FormPopup{control, rml + "</div>"};
        }
        EndRow();
    }

    void Form::NumberAt(const char* name, const char* id, int& value, int limit, bool live)
    {
        const std::string number = Id(id);
        if (const FormEvent* typed = Took(number, kCommitEvent); typed != nullptr && !m_Off)
        {
            char* end     = nullptr;
            const long to = std::strtol(typed->value.c_str(), &end, 10);
            if (end != typed->value.c_str())
            {
                const int clamped = static_cast<int>(std::clamp<long>(to, -limit, limit));
                changed |= clamped != value;
                value = clamped;
            }
        }
        Out() += "<div class=\"num-name\" id=\"" + number + "-name\">" + EscapedRml(name) +
                 "</div><input type=\"text\" class=\"num\" id=\"" + number + "\" maxlength=\"" + std::to_string(kNumberDigits) + "\"/>";
        Set(number + "-name", FormValueKind::Class, kOffClass, {}, m_Off);
        Set(number, FormValueKind::Value, {}, std::to_string(value));
        Set(number, FormValueKind::Class, kOffClass, {}, m_Off);
        Set(number, FormValueKind::Class, kLiveClass, {}, live);
    }

    void Form::Offset(const char* label, int& x, int& y, int limit, const char* tip)
    {
        Row(label, tip);
        NumberAt("X", (std::string(label) + " x").c_str(), x, limit);
        NumberAt("Y", (std::string(label) + " y").c_str(), y, limit);
        EndRow();
    }

    int Form::Tabs(const char* id, int picked, const char* const* labels, int count, int perRow)
    {
        const std::string base = Id(id);
        const int columns      = perRow > 0 ? perRow : count;
        int chosen             = picked;
        Out() += "<div class=\"tabs\" id=\"" + base + "\">";
        for (int tab = 0; tab < count; ++tab)
        {
            const std::string name = base + kTabIdInfix + std::to_string(tab);
            if (Clicked(name)) chosen = tab;
            if (tab % columns == 0) Out() += "<div class=\"tab-row\">";
            Out() += "<div class=\"tab\" id=\"" + name + "\">" + EscapedRml(labels[tab]) + "</div>";
            if (tab % columns == columns - 1 || tab == count - 1) Out() += "</div>";
        }
        Out() += "<div class=\"tab-rule\"></div></div>";
        for (int tab = 0; tab < count; ++tab)
            Set(base + kTabIdInfix + std::to_string(tab), FormValueKind::Class, kPickedClass, {}, tab == chosen);
        return chosen;
    }

    void Form::Note(const char* text)
    {
        const std::string note = Id(kNoteName);
        Out() += "<div class=\"note\" id=\"" + note + "\">" + EscapedRml(text) + "</div>";
        Set(note, FormValueKind::Class, kOffClass, {}, m_Off);
    }

    bool Form::StepArrow(const std::string& label, bool next)
    {
        return Pressable(next ? "<div class=\"tri right\"></div>" : "<div class=\"tri left\"></div>",
            (label + (next ? " next" : " previous")).c_str(), "btn step", nullptr);
    }

    bool Form::Pressable(const std::string& inside, const char* id, const char* classes, const char* tip)
    {
        const std::string button = Id(id);
        Out() += std::string("<div class=\"") + classes + "\" id=\"" + button + "\"" + TipAttribute(tip) + ">" + inside +
                 "</div>";
        Set(button, FormValueKind::Class, kOffClass, {}, m_Off);
        return !m_Off && Clicked(button);
    }

    bool Form::Button(const char* label, const char* id, const char* classes, const char* tip)
    {
        return Pressable(EscapedRml(LabelText(label)), id, classes, tip);
    }

    bool Form::Action(const char* label, const char* tip)
    {
        Line();
        const bool pressed = Button(label, label, kRaisedPill, tip);
        EndLine();
        return pressed;
    }

    void Form::Reading(const char* label, const std::string& text)
    {
        Row(label, nullptr);
        const std::string reading = Id(label);
        Out() += "<div class=\"reading\" id=\"" + reading + "\"></div>";
        Set(reading, FormValueKind::Text, {}, text);
        EndRow();
    }

    bool Form::TextAt(const char* id, std::string& text, const char* hint, float width, bool live, size_t most)
    {
        const std::string box = Id(id);
        bool took             = false;
        if (const FormEvent* typed = Took(box, live ? kChangeEvent : kCommitEvent); typed != nullptr && !m_Off)
        {
            took = live || typed->value != text;
            text = typed->value;
        }
        Out() += "<input type=\"text\" id=\"" + box + "\" style=\"width:" + Em(width) + ";\"";
        if (hint != nullptr) Out() += " placeholder=\"" + EscapedRml(hint) + "\"";
        if (most > 0) Out() += " maxlength=\"" + std::to_string(most) + "\"";
        Out() += "/>";
        Set(box, FormValueKind::Value, {}, text);
        Set(box, FormValueKind::Class, kOffClass, {}, m_Off);
        return took;
    }

    namespace
    {

        // The slot a dropped chip carries; nothing for anything else dropped.
        TagSlot DroppedSlot(const FormEvent* drop)
        {
            if (drop == nullptr) return {};
            const TemplateRuns carried = RunsOf(drop->carried);
            const bool oneSlot = carried.slots.size() == 1 && carried.texts.front().empty() && carried.texts.back().empty();
            return oneSlot ? carried.slots.front() : TagSlot{};
        }

        // A tag as something to drag: its name after its icon when it draws one, and the tag a drop of it brings.
        std::string TagMarkup(const WindowLayout& layout, const std::string& id, Tag tag, const char* classes, const char* tip)
        {
            const LayoutOption& word = layout.OptionsOf(WindowList::Tags)[static_cast<size_t>(tag)];
            const std::optional<Icon> icon = TagIcon(tag);
            return std::string("<div class=\"") + classes + "\" id=\"" + id + "\" " + kCarriesAttribute + "=\"" +
                   EscapedRml(TagName(tag)) + "\"" + TipAttribute(tip) + ">" +
                   (icon ? "<img src=\"" + IconSource(*icon) + "\"/>" : std::string()) + EscapedRml(Said(word.name)) + "</div>";
        }

        // The id of one tag of a line's chip, which is the chip's own while the chip holds no other.
        std::string TagIdOf(const std::string& chip, const TagSlot& slot, int tag)
        {
            return slot.size() == 1 ? chip : chip + kChipTagInfix + std::to_string(tag);
        }

        // A line's slot as a chip: one tag is the chip, and several are parts of it, each dragged alone.
        std::string SlotMarkup(const WindowLayout& layout, const std::string& id, const TagSlot& slot)
        {
            const auto tips = layout.OptionsOf(WindowList::Tags);
            if (slot.size() == 1)
                return TagMarkup(layout, id, slot.front(), slot.front() == Tag::Break ? "btn accent chip in drag break" : "btn accent chip in drag",
                    tips[static_cast<size_t>(slot.front())].tip);
            const char* shared = layout.WordOf(WindowWord::SlotTip).tip;
            std::string parts;
            for (size_t tag = 0; tag < slot.size(); ++tag)
            {
                if (tag > 0) parts += "<div class=\"or\">" + EscapedRml(layout.WordOf(WindowWord::Or).name) + "</div>";
                parts += TagMarkup(layout, TagIdOf(id, slot, static_cast<int>(tag)), slot[tag], "part drag", shared);
            }
            return "<div class=\"btn accent chip in\" id=\"" + id + "\"" + TipAttribute(shared) + ">" + parts + "</div>";
        }
    }

    bool Form::LineBox(const char* id, std::string& text, LineDraft& draft)
    {
        const std::string box = Id(id);
        const auto runId      = [&](int run) { return box + kRunIdInfix + std::to_string(run); };
        const auto chipId     = [&](int chip) { return box + kChipIdInfix + std::to_string(chip); };
        // What the player did is told by the stretches and tags the box had when it was done: one change to those
        // is taken a pass, and whatever else was told of them with it is let go.
        const TemplateRuns had = RunsOf(text);
        const int runs         = static_cast<int>(had.texts.size());
        const std::string was  = text;
        // Asked each pass and never kept: a box built anew loses the keyboard without saying so.
        draft.typing = focused.starts_with(box + kRunIdInfix);
        bool reshaped          = false;
        for (int run = 0; run < runs && !reshaped; ++run)
        {
            const std::string input = runId(run);
            if (const FormEvent* caret = Took(input, kCaretEvent))
                draft.caret = RunCaret{run, static_cast<size_t>(std::strtoul(caret->value.c_str(), nullptr, 10))};
            if (const FormEvent* kill = Took(input, kKillEvent); kill != nullptr && !kill->value.empty())
            {
                // The stretch as it stands typed, then without the tag before it or after it.
                const bool back = kill->value.front() == kKillBack;
                const int tag   = back ? run - 1 : run;
                const std::string typed = WithRun(text, run, std::string_view(kill->value).substr(1));
                const TemplateRuns now  = RunsOf(typed);
                // Where the keyboard stays when a chip of several tags only loses its last.
                RunCaret at{run, back || now.slots != had.slots ? 0 : now.texts[static_cast<size_t>(run)].size()};
                // Typing that made or unmade a tag has the numbers mean something else: it is kept, and the tag stays.
                text     = now.slots == had.slots ? WithoutTag(typed, tag, kLastTag, at) : typed;
                reshaped = true;
                if (RunsOf(text).slots != had.slots && RunsOf(typed).slots == had.slots)
                {
                    draft.caret = at;
                    focus       = FormFocus{runId(at.run), at.at};
                }
            }
            else if (const FormEvent* typed = Took(input, kCommitEvent))
            {
                text     = WithRun(text, run, typed->value);
                reshaped = RunsOf(text).slots != had.slots;
            }
            else if (const FormEvent* enter = Took(input, kBreakEvent))
            {
                // The stretch as it stands typed, then a break where the caret was, and the keyboard on the row
                // after it; typing that made or unmade a tag, or a line with no room for one more, keeps the typing.
                text     = WithRun(text, run, enter->value);
                reshaped = true;
                RunCaret at{run, enter->at};
                if (std::string broken = text; RunsOf(text).slots == had.slots && PutTag(broken, at, {Tag::Break}))
                {
                    text        = std::move(broken);
                    draft.caret = at;
                    focus       = FormFocus{runId(at.run), at.at};
                }
            }
        }
        // Where a chip can be let go in the box: on the box, a stretch, a chip, or one tag of a chip of several.
        struct Place
        {
            std::string id;
            int run = -1, slot = -1, tag = -1;
        };
        std::vector<Place> places{Place{box}};
        // One of the line's own tags dragged from its place: it leaves it, wherever it was let go.
        std::optional<Place> taken;
        for (int run = 0; run < runs; ++run)
        {
            places.push_back(Place{runId(run), run});
            if (run == runs - 1) break;
            const TagSlot& slot = had.slots[static_cast<size_t>(run)];
            if (slot.size() > 1) places.push_back(Place{chipId(run), -1, run});
            for (int tag = 0; tag < static_cast<int>(slot.size()); ++tag)
            {
                places.push_back(Place{TagIdOf(chipId(run), slot, tag), -1, run, tag});
                if (Took(places.back().id, kDragEndEvent) != nullptr) taken = places.back();
            }
        }
        RunCaret nowhere;
        // A tag dropped on a stretch goes in at the pointer, on a chip into that chip's slot, on another tag of its own
        // chip into that tag's place, and anywhere else in the box at its end.
        for (const Place& place : places)
        {
            if (reshaped) break;
            const FormEvent* drop = Took(place.id, kDropEvent);
            const TagSlot dropped = DroppedSlot(drop);
            if (dropped.empty()) continue;
            const bool own = taken && drop->value == taken->id;
            reshaped       = true;
            if (place.slot >= 0)
            {
                if (own && place.slot == taken->slot)
                    MoveTag(text, place.slot, taken->tag, place.tag);
                // Nothing joined is nothing moved: the chip already holds what was dragged.
                else if (JoinSlot(text, place.slot, dropped) && own)
                    text = WithoutTag(text, taken->slot, taken->tag, nowhere);
                continue;
            }
            RunCaret at = place.run < 0 ? RunCaret{} : RunCaret{place.run, drop->at};
            if (!PutTag(text, at, dropped)) continue;
            // The slot went in before the stretch the caret is in: one of the line's own then leaves the place it
            // had, which is a slot further on when the new one stands before it.
            if (own)
            {
                const int put    = at.run - 1;
                const bool whole = had.slots[static_cast<size_t>(taken->slot)].size() == 1;
                text             = WithoutTag(text, taken->slot + (put <= taken->slot ? 1 : 0), taken->tag, nowhere);
                if (whole && taken->slot < put) --at.run;
            }
            draft.caret = at;
        }
        if (taken && !reshaped) text = WithoutTag(text, taken->slot, taken->tag, nowhere);
        const bool edited = text != was;

        const TemplateRuns shown = RunsOf(text);
        Out() += std::string("<div class=\"") + kLineBoxClass + "\" id=\"" + box + "\">";
        for (size_t run = 0; run < shown.texts.size(); ++run)
        {
            const std::string input = runId(static_cast<int>(run));
            Out() += std::string("<input type=\"text\" class=\"") + kRunClass + "\" id=\"" + input + "\"/>";
            Set(input, FormValueKind::Value, {}, shown.texts[run]);
            if (run < shown.slots.size()) Out() += SlotMarkup(*layout, chipId(static_cast<int>(run)), shown.slots[run]);
        }
        Out() += "</div>";
        Set(box, FormValueKind::Class, kOffClass, {}, m_Off);
        changed |= edited;
        return edited;
    }

    bool Form::TagChip(Tag tag)
    {
        const std::string chip = kTagIdStart + std::to_string(static_cast<int>(tag));
        Out() += TagMarkup(*layout, chip, tag, "btn accent chip drag", layout->OptionsOf(WindowList::Tags)[static_cast<size_t>(tag)].tip);
        Set(chip, FormValueKind::Class, kOffClass, {}, m_Off);
        return !m_Off && Clicked(chip);
    }

    void Form::Caption(const char* text, const char* classes, float width)
    {
        const std::string caption = Id(std::string(text) + kCaptionSuffix);
        Out() += std::string("<div class=\"") + classes + "\" id=\"" + caption + "\"";
        if (width > 0.0f) Out() += " style=\"width:" + Em(width) + ";\"";
        Out() += ">" + EscapedRml(LabelText(text)) + "</div>";
        Set(caption, FormValueKind::Class, kOffClass, {}, m_Off);
    }
}
