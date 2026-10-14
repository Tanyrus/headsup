#include "form.h"

#include "argb.h"
#include "bar_document.h"
#include "menu_text.h"
#include "settings.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <utility>

namespace headsup
{
    namespace
    {
        constexpr const char* kIdMark     = "##"; // in a label: what follows keeps controls apart and is not shown
        constexpr float kColorFull        = 255.0f;
        constexpr int kHexDigits          = 6;
        constexpr int kHexBase            = 16;
        constexpr const char* kChannels[] = {"r", "g", "b"};
        constexpr const char* kChannelNames[] = {"R", "G", "B"};
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

        std::string TipAttribute(std::string_view tip)
        {
            return tip.empty() ? std::string() : " tip=\"" + EscapedRml(tip) + "\"";
        }
        std::string TipAttribute(const char* tip) { return TipAttribute(std::string_view(tip != nullptr ? tip : "")); }

        std::string Formatted(const char* format, double value)
        {
            char text[64];
            std::snprintf(text, sizeof(text), format, value);
            return text;
        }
        std::string Formatted(const char* format, int value)
        {
            char text[64];
            std::snprintf(text, sizeof(text), format, value);
            return text;
        }

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
        std::string id = m_CardId.empty() ? Slug(name) : m_CardId + "-" + Slug(name);
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
        return m_CardId.empty() ? Slug(name) : m_CardId + "-" + Slug(name);
    }

    const FormEvent* Form::Took(std::string_view id, const char* type) const
    {
        for (auto event = m_Events.rbegin(); event != m_Events.rend(); ++event)
            if (event->id == id && event->type == type) return &*event;
        return nullptr;
    }

    void Form::Set(std::string id, FormValueKind kind, std::string name, std::string text, bool on)
    {
        values.push_back(FormValue{std::move(id), kind, std::move(name), std::move(text), on});
    }

    std::string Form::Shown(const char* label)
    {
        const char* mark = std::strstr(label, kIdMark);
        return TitleCaseText(std::string_view(label, mark != nullptr ? static_cast<size_t>(mark - label) : std::strlen(label)));
    }

    void Form::BeginBlock(const char* id)
    {
        blocks.push_back(FormBlock{id, {}});
        m_CardId = id;
    }

    bool Form::BeginCard(const char* title, bool* enabled, const char* about)
    {
        m_CardId = Slug(pageKey + "/" + title);
        blocks.push_back(FormBlock{m_CardId, {}});
        Set(m_CardId, FormValueKind::Class, "off", {}, m_Off);
        if (!m_Off) ++m_CardsShown;
        const std::string head = m_CardId + "-head";
        if (!m_Off && Clicked(head) && !m_State.collapsed.erase(m_CardId)) m_State.collapsed.insert(m_CardId);
        const bool open = !m_Off && !m_State.collapsed.contains(m_CardId);
        Out() += "<div class=\"card-head\" id=\"" + head + "\"><div class=\"arrow" + (open ? " open" : "") +
                 "\"></div><div class=\"card-title\">" + EscapedRml(title) + "</div>";
        if (enabled != nullptr)
        {
            const std::string check = m_CardId + "-on";
            if (!m_Off && Clicked(check))
            {
                *enabled = !*enabled;
                changed  = true;
            }
            Out() += "<div class=\"check card-on\" id=\"" + check + "\"><div class=\"mark\"></div></div>";
            Set(check, FormValueKind::Class, "on", {}, *enabled);
        }
        Out() += "</div>";
        if (!open) return false;
        Out() += "<div class=\"card-body\">";
        if (about != nullptr && *about != '\0') Out() += "<div class=\"note\">" + EscapedRml(about) + "</div>";
        return true;
    }

    void Form::EndCard() { Out() += "</div>"; }

    void Form::Row(const char* label, const char* tip)
    {
        const std::string id = Id(std::string(label) + " label");
        const std::string row = id + "-row";
        Out() += std::string("<div class=\"row") + (m_Indent ? " indent" : "") + "\" id=\"" + row + "\"><div class=\"label\" id=\"" +
                 id + "\"" + TipAttribute(tip) + ">" + EscapedRml(Shown(label)) + "</div><div class=\"ctl\">";
        Set(row, FormValueKind::Class, "off", {}, m_Off);
    }

    void Form::EndRow() { Out() += "</div></div>"; }

    void Form::Line(const char* classes) { Out() += std::string("<div class=\"") + classes + "\">"; }
    void Form::EndLine() { Out() += "</div>"; }

    std::string Form::Check(const char* id, bool& on, const char* tip)
    {
        const std::string check = Id(id);
        if (!m_Off && Clicked(check))
        {
            on      = !on;
            changed = true;
        }
        Out() += "<div class=\"check\" id=\"" + check + "\"" + TipAttribute(tip) + "><div class=\"mark\"></div></div>";
        Set(check, FormValueKind::Class, "on", {}, on);
        Set(check, FormValueKind::Class, "off", {}, m_Off);
        return check;
    }

    void Form::CheckAt(const char* id, bool& on, const char* tip) { Check(id, on, tip); }

    void Form::Switch(const char* label, bool& on, const char* tip, const char* offWord, const char* onWord)
    {
        Row(label, tip);
        Out() += "<div class=\"switch-room\">";
        const std::string check = Check(label, on, nullptr);
        if (offWord != nullptr && onWord != nullptr)
        {
            Out() += "<div class=\"state\" id=\"" + check + "-state\"></div>";
            Set(check + "-state", FormValueKind::Text, {}, on ? onWord : offWord);
        }
        Out() += "</div>";
        EndRow();
    }

    void Form::SegmentsAt(const char* id, int& value, const char* const* options, int count, float segment)
    {
        const std::string base  = Id(id);
        const std::string width = Em(segment > 0.0f ? segment : kSegmentWidth);
        Out() += "<div class=\"segs\" id=\"" + base + "\">";
        for (int option = 0; option < count; ++option)
        {
            const std::string seg = base + "-s-" + std::to_string(option);
            if (!m_Off && Clicked(seg) && value != option)
            {
                value   = option;
                changed = true;
            }
            Out() += "<div class=\"seg\" id=\"" + seg + "\" style=\"width:" + width + ";\">" +
                     EscapedRml(TitleCaseText(options[option])) + "</div>";
        }
        Out() += "</div>";
        for (int option = 0; option < count; ++option)
            Set(base + "-s-" + std::to_string(option), FormValueKind::Class, "picked", {}, value == option);
        Set(base, FormValueKind::Class, "off", {}, m_Off);
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
        Set(slider + "-box", FormValueKind::Class, "off", {}, m_Off);
        EndRow();
    }

    bool Form::Opened(std::string_view name) const
    {
        const std::string control = Named(name);
        return m_State.popup == control && Clicked(control);
    }

    void Form::TakeWhole(const std::string& slider, int& value, int lo, int hi)
    {
        const FormEvent* moved = Took(slider, kChangeEvent);
        if (moved == nullptr || m_Off) return;
        const int to = std::clamp(static_cast<int>(std::lround(std::strtof(moved->value.c_str(), nullptr))), lo, hi);
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

    bool Form::ListControl(const std::string& control, const std::string& shown, float width, const std::string& tip, bool markup)
    {
        if (!m_Off && Clicked(control))
        {
            if (m_State.popup == control)
                m_State.popup.clear();
            else
                m_State.popup = control;
        }
        const bool open = m_State.popup == control;
        Out() += "<div class=\"dd\" id=\"" + control + "\"" + TipAttribute(tip) + " style=\"width:" +
                 Em(width > 0.0f ? width : kControlWidth) + ";\"><div class=\"dd-val\" id=\"" + control +
                 "-val\"></div><div class=\"dd-arrow\"></div></div>";
        Set(control + "-val", markup ? FormValueKind::Markup : FormValueKind::Text, {}, shown);
        Set(control, FormValueKind::Class, "off", {}, m_Off);
        Set(control, FormValueKind::Class, "open", {}, open);
        return open;
    }

    void Form::MarkupDropdown(const char* label, int& value, const std::vector<std::string>& rows, const char* tip,
        const OptionTip& option)
    {
        Row(label, tip);
        const std::string control = Id(label);
        const int count           = static_cast<int>(rows.size());
        const int picked          = std::clamp(value, 0, count - 1);
        if (ListControl(control, rows[static_cast<size_t>(picked)], 0.0f, option ? option(picked) : std::string(), true))
        {
            std::string rml;
            for (int i = 0; i < count; ++i)
                rml += ListRow(control, i, rows[static_cast<size_t>(i)], i == picked, option ? option(i) : std::string(), true);
            int chosen = picked;
            if (ListChoice(control, std::move(rml), count, chosen) && chosen != picked)
            {
                value   = chosen;
                changed = true;
            }
        }
        EndRow();
    }

    std::string Form::ListRow(const std::string& control, int index, const std::string& name, bool picked,
        const std::string& tip, bool markup)
    {
        const std::string row = control + "-o-" + std::to_string(index);
        Set(row, FormValueKind::Class, "picked", {}, picked);
        return "<div class=\"opt\" id=\"" + row + "\"" + TipAttribute(tip) + ">" + (markup ? name : EscapedRml(name)) + "</div>";
    }

    bool Form::ListChoice(const std::string& control, std::string rows, int count, int& chosen)
    {
        const std::string start = control + "-o-";
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
        const std::string base  = Id(label);
        const std::string width = Em(segment > 0.0f ? segment : kSegmentWidth);
        Out() += "<div class=\"segs\" id=\"" + base + "\">";
        for (int option = 0; option < count; ++option)
        {
            const std::string seg = base + "-s-" + std::to_string(option);
            const auto bit        = static_cast<uint8_t>(1u << option);
            if (!m_Off && Clicked(seg))
            {
                bits ^= bit;
                changed = true;
            }
            Out() += "<div class=\"seg\" id=\"" + seg + "\" style=\"width:" + width + ";\">" + EscapedRml(options[option]) + "</div>";
            Set(seg, FormValueKind::Class, "picked", {}, (bits & bit) != 0);
        }
        Out() += "</div>";
        Set(base, FormValueKind::Class, "off", {}, m_Off);
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
        if (Pressable("<div class=\"tri left\"></div>", (name + " previous").c_str(), "btn step", nullptr))
            chosen = StepFontEntry(count, picked, -1);
        int listed = 0;
        // The arrows take their room from the dropdown, so the row ends where a slider's does.
        const float step = 1.6f + 0.5f;
        // A long family's name is cut in the list and in the closed control: both say it whole under the pointer.
        const std::string shown = FontShown(choices, unset, font);
        if (ListAt(label, shown, kControlWidth - 2.0f * step, count,
                [&](int entry) { return FontEntryLabel(choices, unset, entry); }, picked, listed, shown.c_str(),
                [&](int entry) { return FontEntryLabel(choices, unset, entry); }))
            chosen = listed;
        if (Pressable("<div class=\"tri right\"></div>", (name + " next").c_str(), "btn step", nullptr))
            chosen = StepFontEntry(count, picked, 1);
        if (chosen != picked && chosen != kNoFontEntry)
        {
            font    = FontOfEntry(choices, unset, chosen);
            changed = true;
        }
        EndRow();
    }

    void Form::SwatchAt(const char* id, Color& color)
    {
        const std::string swatch = Id(id);
        if (!m_Off && Clicked(swatch))
        {
            if (m_State.popup == swatch)
                m_State.popup.clear();
            else
                m_State.popup = swatch;
        }
        Out() += "<div class=\"swatch\" id=\"" + swatch + "\"></div>";
        if (m_State.popup == swatch)
        {
            // Its three channels as sliders, and the color as it is written, to type.
            std::string rml = "<div class=\"picker\">";
            for (int channel = 0; channel < 3; ++channel)
            {
                const std::string slider = swatch + "-p-" + kChannels[channel];
                // A slider says its value as it is made and each time it is set: only a level other than the color's
                // own is the player's, or opening the picker would round the color to it.
                const FormEvent* moved = Took(slider, kChangeEvent);
                const float to         = moved != nullptr ? std::clamp(std::strtof(moved->value.c_str(), nullptr), 0.0f, kColorFull) : 0.0f;
                if (moved != nullptr && std::lround(to) != std::lround(color.v[channel] * kColorFull))
                {
                    color.v[channel] = to / kColorFull;
                    changed          = true;
                }
                const int level = static_cast<int>(std::lround(color.v[channel] * kColorFull));
                rml += std::string("<div class=\"pick-row\"><div class=\"pick-name\">") + kChannelNames[channel] +
                       "</div><input type=\"range\" id=\"" + slider + "\" min=\"0\" max=\"255\" step=\"1\"/>" +
                       "<div class=\"val\" id=\"" + slider + "-val\"></div></div>";
                Set(slider, FormValueKind::Value, {}, std::to_string(level));
                Set(slider + "-val", FormValueKind::Text, {}, std::to_string(level));
            }
            const std::string hex = swatch + "-p-hex";
            if (const FormEvent* typed = Took(hex, kCommitEvent))
            {
                const char* text = typed->value.c_str() + (typed->value.starts_with("#") ? 1 : 0);
                char* end        = nullptr;
                const unsigned long rgb = std::strtoul(text, &end, kHexBase);
                if (end == text + kHexDigits && *end == '\0')
                {
                    color   = ColorFromRgb(static_cast<uint32_t>(rgb));
                    changed = true;
                }
            }
            rml += "<div class=\"pick-row\"><div class=\"pick-name\">#</div><input type=\"text\" class=\"hex\" id=\"" + hex +
                   "\" maxlength=\"7\"/></div></div>";
            char written[16];
            std::snprintf(written, sizeof(written), "%06X", static_cast<unsigned>(RgbOf(color)));
            Set(hex, FormValueKind::Value, {}, written);
            popup = FormPopup{swatch, std::move(rml)};
        }
        Set(swatch, FormValueKind::Property, "background-color", RcssColor(Opaque(RgbOf(color))));
        Set(swatch, FormValueKind::Class, "off", {}, m_Off);
    }

    void Form::Swatch(const char* label, Color& color, const char* tip, const Color* original)
    {
        Row(label, tip);
        if (original != nullptr)
        {
            const std::string reset = Id(std::string(label) + " reset");
            if (!m_Off && Clicked(reset) && color != *original)
            {
                color   = *original;
                changed = true;
            }
            // In the markup either way, hidden while there is nothing to put back: a color changed builds nothing.
            Out() += "<div class=\"reset\" id=\"" + reset + "\" tip=\"Back to its default.\"></div>";
            Set(reset, FormValueKind::Class, "hidden", {}, color == *original);
        }
        SwatchAt(label, color);
        EndRow();
    }

    void Form::ColorPickRow(const char* label, ColorPick& pick, const std::vector<ColorSource>& sources, const char* tip)
    {
        Row(label, tip);
        const auto at = std::ranges::find(sources, pick.source);
        int chosen    = 0;
        // The swatch takes its room from the dropdown, which keeps its place for any other source.
        const float swatch = 1.6f + 0.5f;
        if (ListAt(label, SourceLabel(pick.source), kControlWidth - swatch, static_cast<int>(sources.size()),
                [&](int i) { return std::string(SourceLabel(sources[static_cast<size_t>(i)])); },
                at != sources.end() ? static_cast<int>(at - sources.begin()) : -1, chosen, nullptr,
                [&](int i) { return std::string(SourceTip(sources[static_cast<size_t>(i)])); }))
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

    void Form::ChecksDropdown(const char* label, LineShow& show, const std::vector<ShowCondition>& conditions,
        const char* tip)
    {
        Row(label, tip);
        const std::string control = Id(label);
        const std::string start   = control + "-o-";
        // Before the control is written, so its summary is what the click just made it.
        if (m_State.popup == control)
            for (const FormEvent& event : m_Events)
            {
                if (event.type != kClickEvent || event.id.compare(0, start.size(), start) != 0) continue;
                changed |= ToggleCondition(show, conditions, std::atoi(event.id.c_str() + start.size()));
            }
        if (ListControl(control, ShowSummary(show, conditions), kControlWidth))
        {
            std::string rml = "<div class=\"list\">";
            const auto row  = [&](int index, const char* name, const char* tip, bool on) {
                const std::string id = start + std::to_string(index);
                rml += "<div class=\"opt chk\" id=\"" + id + "\"" + TipAttribute(tip) + "><div class=\"dot\"></div>" +
                       EscapedRml(name) + "</div>";
                Set(id, FormValueKind::Class, "on", {}, on);
            };
            row(0, kShowAlways, kShowAlwaysTip, show.always);
            for (size_t i = 0; i < conditions.size(); ++i)
                row(static_cast<int>(i) + 1, ConditionLabel(conditions[i]), ConditionTip(conditions[i]),
                    show.always || (show.when & ConditionBit(conditions[i])) != 0);
            popup = FormPopup{control, rml + "</div>"};
        }
        EndRow();
    }

    void Form::NumberAt(const char* name, const char* id, int& value, int limit)
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
        Out() += std::string("<div class=\"num-name") + OffClass() + "\">" + EscapedRml(name) +
                 "</div><input type=\"text\" class=\"num\" id=\"" + number + "\" maxlength=\"6\"/>";
        Set(number, FormValueKind::Value, {}, std::to_string(value));
        Set(number, FormValueKind::Class, "off", {}, m_Off);
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
            const std::string name = base + "-t-" + std::to_string(tab);
            if (Clicked(name)) chosen = tab;
            if (tab % columns == 0) Out() += "<div class=\"tab-row\">";
            Out() += "<div class=\"tab\" id=\"" + name + "\">" + EscapedRml(labels[tab]) + "</div>";
            if (tab % columns == columns - 1 || tab == count - 1) Out() += "</div>";
        }
        Out() += "<div class=\"tab-rule\"></div></div>";
        for (int tab = 0; tab < count; ++tab)
            Set(base + "-t-" + std::to_string(tab), FormValueKind::Class, "picked", {}, tab == chosen);
        return chosen;
    }

    void Form::Note(const char* text)
    {
        Out() += std::string("<div class=\"note") + OffClass() + "\">" + EscapedRml(text) + "</div>";
    }

    bool Form::Pressable(const std::string& inside, const char* id, const char* classes, const char* tip)
    {
        const std::string button = Id(id);
        Out() += std::string("<div class=\"") + classes + "\" id=\"" + button + "\"" + TipAttribute(tip) + ">" + inside +
                 "</div>";
        Set(button, FormValueKind::Class, "off", {}, m_Off);
        return !m_Off && Clicked(button);
    }

    bool Form::Button(const char* label, const char* id, const char* classes, const char* tip)
    {
        return Pressable(EscapedRml(label), id, classes, tip);
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
        Set(box, FormValueKind::Class, "off", {}, m_Off);
        return took;
    }

    namespace
    {
        constexpr const char* kSlotTip = "These share one place, tried from the left: the first that has something to show "
                                         "is drawn. Drop one on another to change their order, or drag one out.";
        constexpr const char* kSlotJoin = "or"; // between the tags of a chip of several

        // The slot a dropped chip carries; nothing for anything else dropped.
        TagSlot DroppedSlot(const FormEvent* drop)
        {
            if (drop == nullptr) return {};
            const TemplateRuns carried = RunsOf(drop->carried);
            const bool oneSlot = carried.slots.size() == 1 && carried.texts.front().empty() && carried.texts.back().empty();
            return oneSlot ? carried.slots.front() : TagSlot{};
        }

        // A tag as something to drag: its name after its icon when it draws one, and the tag a drop of it brings.
        std::string TagMarkup(const std::string& id, Tag tag, const char* classes, const char* tip)
        {
            const std::optional<Icon> icon = TagIcon(tag);
            return std::string("<div class=\"") + classes + "\" id=\"" + id + "\" " + kCarriesAttribute + "=\"" +
                   EscapedRml(TagName(tag)) + "\"" + TipAttribute(tip) + ">" +
                   (icon ? "<img src=\"" + IconSource(*icon) + "\"/>" : std::string()) + EscapedRml(TagLabel(tag)) + "</div>";
        }

        // The id of one tag of a line's chip, which is the chip's own while the chip holds no other.
        std::string TagIdOf(const std::string& chip, const TagSlot& slot, int tag)
        {
            return slot.size() == 1 ? chip : chip + kChipTagInfix + std::to_string(tag);
        }

        // A line's slot as a chip: one tag is the chip, and several are parts of it, each dragged alone.
        std::string SlotMarkup(const std::string& id, const TagSlot& slot)
        {
            if (slot.size() == 1) return TagMarkup(id, slot.front(), "btn accent chip in drag", TagTip(slot.front()));
            std::string parts;
            for (size_t tag = 0; tag < slot.size(); ++tag)
            {
                if (tag > 0) parts += std::string("<div class=\"or\">") + kSlotJoin + "</div>";
                parts += TagMarkup(TagIdOf(id, slot, static_cast<int>(tag)), slot[tag], "part drag", kSlotTip);
            }
            return "<div class=\"btn accent chip in\" id=\"" + id + "\"" + TipAttribute(kSlotTip) + ">" + parts + "</div>";
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
        // A tag dropped on a stretch goes after it, on a chip into that chip's slot, on another tag of its own chip
        // into that tag's place, and anywhere else in the box at its end.
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
            RunCaret at = place.run < 0 ? RunCaret{} : RunCaret{place.run, std::string::npos};
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
        Out() += "<div class=\"linebox\" id=\"" + box + "\">";
        for (size_t run = 0; run < shown.texts.size(); ++run)
        {
            const std::string input = runId(static_cast<int>(run));
            Out() += "<input type=\"text\" class=\"run\" id=\"" + input + "\"/>";
            Set(input, FormValueKind::Value, {}, shown.texts[run]);
            if (run < shown.slots.size()) Out() += SlotMarkup(chipId(static_cast<int>(run)), shown.slots[run]);
        }
        Out() += "</div>";
        Set(box, FormValueKind::Class, "off", {}, m_Off);
        changed |= edited;
        return edited;
    }

    bool Form::TagChip(Tag tag)
    {
        const std::string chip = kTagIdStart + std::to_string(static_cast<int>(tag));
        Out() += TagMarkup(chip, tag, "btn accent chip drag", TagTip(tag));
        Set(chip, FormValueKind::Class, "off", {}, m_Off);
        return !m_Off && Clicked(chip);
    }

    void Form::Caption(const char* text, const char* classes, float width)
    {
        Out() += std::string("<div class=\"") + classes + OffClass() + "\"";
        if (width > 0.0f) Out() += " style=\"width:" + Em(width) + ";\"";
        Out() += ">" + EscapedRml(text) + "</div>";
    }
}
