#pragma once

#include "Ashita.h"
#include "bar_placement.h"
#include "bar_theme.h"
#include "menu_fonts.h"
#include "menu_sizes.h"
#include "menu_state.h"
#include "plate_template.h"
#include "settings.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace headsup
{
    constexpr ImVec4 Hex(uint32_t rgb, float alpha = 1.0f)
    {
        return ImVec4(static_cast<float>((rgb >> 16) & 0xFF) / 255.0f, static_cast<float>((rgb >> 8) & 0xFF) / 255.0f,
            static_cast<float>(rgb & 0xFF) / 255.0f, alpha);
    }

    // The Phoenix palette of phoenix-xi.com, which the bar's Phoenix themes share: a page, the cards on it, the
    // controls on those, each a step lighter, and the trays a step below the page.
    constexpr ImVec4 kBackground  = Hex(0x150C0C, 0.97f);
    constexpr ImVec4 kCard        = Hex(0x24181A);
    constexpr ImVec4 kControl     = Hex(0x352325);
    constexpr ImVec4 kHover       = Hex(0x452D2F);
    constexpr ImVec4 kInset       = Hex(0x0F0808);
    constexpr ImVec4 kBorder      = Hex(0xD2ABAB, 0.14f);
    constexpr ImVec4 kRaisedEdge  = Hex(0xD2ABAB, 0.24f); // a chip's edge, lighter than a card's
    constexpr ImVec4 kText        = Hex(0xFFF4F0);
    constexpr ImVec4 kHeading     = Hex(0xE7C9A9); // the bar's gold titles
    constexpr ImVec4 kLabel       = Hex(0xD9C2BE);
    constexpr ImVec4 kMuted       = Hex(0x8C7270);
    constexpr ImVec4 kAccent      = Hex(kPhoenixRed);
    constexpr ImVec4 kAccentHover = Hex(0xE06F5E);
    constexpr ImVec4 kAccentSoft  = Hex(0xC55151, 0.28f); // the picked page, a hovered row
    constexpr ImVec4 kClear       = Hex(0x000000, 0.0f);

    struct ButtonColors
    {
        ImVec4 normal, hovered, active;
    };
    constexpr ButtonColors kFlat{kClear, kAccentSoft, kAccentSoft};
    constexpr ButtonColors kStill{kCard, kCard, kCard}; // a card's ground: no hover
    constexpr ButtonColors kTray{kInset, kInset, kInset};
    constexpr ButtonColors kRaised{kControl, kHover, kAccentSoft};
    constexpr ButtonColors kPickedPill{kAccentSoft, kAccentSoft, kAccentSoft};
    constexpr ButtonColors kAccentButton{kAccent, kAccentHover, kAccent};

    constexpr const char* kWidgetPayload = "HU_BAR_WIDGET";
    constexpr const char* kTagPayload    = "HU_TAG"; // a dragged tag chip carries its Tag
    constexpr const char* kBoldTip       = "Bold";

    // What a dragged chip carries: where it came from.
    struct DraggedWidget
    {
        uint8_t section = 0;
        uint8_t index   = 0;
    };

    struct Drop
    {
        BarSection from;
        size_t index;
        BarSection to;
        size_t at;
    };

    // A label as the menu shows it: every word capitalized, after a space or a hyphen ("Sub-Target", "24-Hour").
    std::string TitleCase(const char* begin, const char* end);
    std::string TitleCase(const char* text);

    void ApplyPhoenixStyle(ImGuiStyle& s, float em);

    // A text box's caret, in bytes, into the size_t it is handed, each frame it is typed in. ImGui calls this from
    // inside the game: it touches nothing else.
    inline int NoteCaret(ImGuiInputTextCallbackData* data)
    {
        *static_cast<size_t*>(data->UserData) = static_cast<size_t>(data->CursorPos);
        return 0;
    }

    // The menu's drawing kit. PushStyleColor, PushStyleVar, CalcTextSize and the like are overloaded or return a
    // struct, which tools/abi_check.py refuses, so colors and sizes are set in the style while an item draws and
    // controls are placed from the right edge of their card instead of by their text's width.
    struct Ui
    {
        IGuiManager* gui;
        ImGuiStyle& style;
        float alpha; // the window's own, before fading
        float em;
        ImFont* titleFont;
        std::unordered_map<std::string, float>& cardHeights;
        bool changed = false;
        // The card being filled: its left edge and width, in window coordinates.
        float cardX = 0.0f, cardWidth = 0.0f, cardTop = 0.0f;
        std::string cardId;
        std::string pageKey; // the page being drawn, which keeps two pages' cards of one title apart
        bool disabled = false; // inside Fade(true)'s BeginDisabled

        float Em(float ems) const { return ems * em; }

        template <typename Draw>
        auto WithColors(const ButtonColors& colors, Draw draw)
        {
            ImVec4* c           = style.Colors;
            const ImVec4 was[3] = {c[ImGuiCol_Button], c[ImGuiCol_ButtonHovered], c[ImGuiCol_ButtonActive]};
            c[ImGuiCol_Button]        = colors.normal;
            c[ImGuiCol_ButtonHovered] = colors.hovered;
            c[ImGuiCol_ButtonActive]  = colors.active;
            const auto result         = draw();
            c[ImGuiCol_Button]        = was[0];
            c[ImGuiCol_ButtonHovered] = was[1];
            c[ImGuiCol_ButtonActive]  = was[2];
            return result;
        }

        template <typename Draw>
        auto WithText(const ImVec4& color, Draw draw)
        {
            ImVec4& slot     = style.Colors[ImGuiCol_Text];
            const ImVec4 was = slot;
            slot             = color;
            const auto result = draw();
            slot              = was;
            return result;
        }

        // A rounded button of this rounding, in these colors, with an edge when edge has alpha.
        bool Shape(const char* id, const ImVec2& size, const ButtonColors& colors, float rounding,
            const ImVec4& edge = kClear)
        {
            const float wasRounding = style.FrameRounding, wasBorder = style.FrameBorderSize;
            const ImVec4 wasEdge    = style.Colors[ImGuiCol_Border];
            const ImVec2 wasPadding = style.FramePadding;
            // A shape shorter than its text and padding would grow: its padding shrinks instead.
            if (size.y > 0.0f) style.FramePadding.y = std::max(0.0f, std::min(wasPadding.y, (size.y - gui->GetFontSize()) / 2.0f));
            style.FrameRounding     = rounding;
            style.FrameBorderSize   = edge.w > 0.0f ? 1.0f : 0.0f;
            style.Colors[ImGuiCol_Border] = edge;
            const bool pressed = WithColors(colors, [&] { return gui->Button(id, size); });
            style.FramePadding  = wasPadding;
            style.FrameRounding = wasRounding;
            style.FrameBorderSize = wasBorder;
            style.Colors[ImGuiCol_Border] = wasEdge;
            return pressed;
        }

        // A shape under what is drawn next: it takes no clicks from the items over it.
        void Ground(const char* id, const ImVec2& size, const ButtonColors& colors, float rounding,
            const ImVec4& edge = kClear)
        {
            gui->SetNextItemAllowOverlap();
            Shape(id, size, colors, rounding, edge);
        }

        void Text(const ImVec4& color, const char* text)
        {
            WithText(color, [&] {
                gui->TextUnformatted(text);
                return 0;
            });
        }

        // Text in the title font, at ems of the menu's size.
        void Title(const ImVec4& color, const char* text, float ems)
        {
            if (titleFont != nullptr) gui->PushFont(titleFont, Em(ems));
            Text(color, text);
            if (titleFont != nullptr) gui->PopFont();
        }

        void Tip(const char* tip)
        {
            if (tip == nullptr || !gui->IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) return;
            const float faded = style.Alpha;
            style.Alpha       = alpha;
            gui->BeginTooltip();
            gui->PushTextWrapPos(Em(kTipWidth));
            gui->TextUnformatted(tip);
            gui->PopTextWrapPos();
            gui->EndTooltip();
            style.Alpha = faded;
        }

        // Faint and unclickable while the feature they belong to is off; its tip still shows on hover.
        void Fade(bool off)
        {
            if (off != disabled)
            {
                if (off)
                    gui->BeginDisabled(true);
                else
                    gui->EndDisabled(); // puts back the alpha BeginDisabled saw, which is set below anyway
                disabled = off;
            }
            style.Alpha = off ? alpha * kOffAlpha : alpha;
        }

        // An item that cannot be used just now: faint, and it takes no clicks.
        template <typename Draw>
        auto Unless(bool off, Draw draw)
        {
            const float was = style.Alpha;
            gui->BeginDisabled(off);
            if (off) style.Alpha = was * kOffAlpha;
            const auto result = draw();
            gui->EndDisabled();
            style.Alpha = was;
            return result;
        }

        // Room for a chip of this many letters, a little over what it takes, for wrapping a row of chips before
        // the card's edge: measuring text returns a struct, which abi_check refuses.
        float ChipRoom(size_t letters) const
        {
            return static_cast<float>(letters) * Em(kLetterWidth) + 2.0f * style.FramePadding.x;
        }

        // Puts the cursor at x, y and ends the line there, so the next item starts on the line after height.
        void EndRowAt(float x, float y, float height)
        {
            gui->SetCursorPos(ImVec2(x, y + height));
            gui->Dummy(ImVec2(0.0f, 0.0f));
        }

        void PageTitle(const char* title)
        {
            const float x = gui->GetCursorPosX(), y = gui->GetCursorPosY();
            Title(kHeading, title, kPageTitleScale);
            EndRowAt(x, y, gui->GetFrameHeight() + Em(0.4f));
        }

        // A rounded card, its arrow or title row folding it by its id (the page and the title); an enabled checkbox
        // sits at the title's right. Its ground takes the height it had last frame.
        bool BeginCard(const char* title, std::unordered_set<std::string>& collapsed, bool* enabled = nullptr,
            const char* tip = nullptr)
        {
            cardX   = gui->GetCursorPosX();
            cardTop = gui->GetCursorPosY();
            cardId  = pageKey + "/" + title;
            const bool open    = !collapsed.contains(cardId);
            const float header = Em(kHeaderRow);
            const auto known   = cardHeights.find(cardId);
            const float height = open && known != cardHeights.end() ? known->second : header;
            char id[96];
            std::snprintf(id, sizeof(id), "##card %s", cardId.c_str());
            Ground(id, ImVec2(cardWidth, height), kStill, Em(kCardRadius), kBorder);
            const float arrowX = cardX + Em(0.5f);
            const float arrow  = gui->GetFrameHeight();
            // The switch at the title row's right keeps its own clicks.
            gui->SetCursorPos(ImVec2(arrowX + arrow, cardTop));
            std::snprintf(id, sizeof(id), "##fold %s", cardId.c_str());
            const float switchRoom = enabled != nullptr ? Em(kSwitchWidth) + Em(kCardPad) : 0.0f;
            const bool rowPressed =
                gui->InvisibleButton(id, ImVec2(cardWidth - switchRoom - (arrowX + arrow - cardX), header));
            const bool hovered = gui->IsItemHovered();
            Tip(tip);
            gui->SetCursorPos(ImVec2(arrowX, cardTop + (header - arrow) / 2.0f));
            std::snprintf(id, sizeof(id), "##arrow %s", cardId.c_str());
            const bool arrowPressed = WithText(hovered ? kText : kMuted, [&] {
                return WithColors(ButtonColors{kClear, kClear, kClear},
                    [&] { return gui->ArrowButton(id, open ? ImGuiDir_Down : ImGuiDir_Right); });
            });
            if (arrowPressed || rowPressed)
            {
                if (!collapsed.erase(cardId)) collapsed.insert(cardId);
            }
            gui->SetCursorPos(ImVec2(arrowX + arrow, cardTop + (header - Em(kCardTitleScale)) / 2.0f));
            Title(kHeading, title, kCardTitleScale);
            if (enabled != nullptr)
            {
                gui->SetCursorPos(ImVec2(cardX + cardWidth - Em(kCardPad) - Em(kSwitchWidth),
                    cardTop + (header - gui->GetFrameHeight()) / 2.0f));
                std::snprintf(id, sizeof(id), "##enabled %s", cardId.c_str());
                SwitchAt(id, *enabled);
            }
            EndRowAt(cardX, cardTop, header);
            if (!open) EndCard();
            return open;
        }

        void EndCard()
        {
            const float bottom = gui->GetCursorPosY() - style.ItemSpacing.y + Em(kCardPad) * 0.6f;
            const float height = std::max(Em(kHeaderRow), bottom - cardTop);
            cardHeights[cardId] = height;
            EndRowAt(cardX, cardTop, height + Em(kGutter) * 0.7f - style.ItemSpacing.y);
        }

        // A row's label at the card's left, its tip on hover; the cursor goes to where a control of width ends
        // at the card's right padding. Returns the row's top.
        float Row(const char* label, const char* tip, float width)
        {
            const float y   = gui->GetCursorPosY();
            const char* id  = std::strstr(label, "##");
            const char* end = id != nullptr ? id : label + std::strlen(label);
            gui->SetCursorPos(ImVec2(cardX + Em(kCardPad), y));
            gui->AlignTextToFramePadding();
            const std::string shown = TitleCase(label, end);
            WithText(kLabel, [&] {
                gui->TextUnformatted(shown.c_str());
                return 0;
            });
            Tip(tip);
            gui->SetCursorPos(ImVec2(cardX + cardWidth - Em(kCardPad) - width, y));
            return y;
        }

        // A checkbox at the right of a switch's width, so what is placed by that width keeps its place.
        void SwitchAt(const char* id, bool& on)
        {
            const float x = gui->GetCursorPosX(), y = gui->GetCursorPosY();
            const float frame = gui->GetFrameHeight();
            gui->SetCursorPos(ImVec2(x + Em(kSwitchWidth) - frame, y));
            if (gui->Checkbox(id, &on)) changed = true;
            EndRowAt(x, y, frame);
        }

        void Switch(const char* label, bool& on, const char* tip)
        {
            const float y = Row(label, tip, Em(kSwitchWidth));
            char id[80];
            std::snprintf(id, sizeof(id), "##switch %s", label);
            SwitchAt(id, on);
            EndRowAt(cardX, y, gui->GetFrameHeight());
        }

        // Options as joined buttons, the picked one filled in the accent.
        template <typename Enum>
        void SegmentsAt(const char* id, Enum& value, const char* const* options, int count, float segment,
            const char* tip = nullptr)
        {
            const float x = gui->GetCursorPosX(), y = gui->GetCursorPosY();
            const float frame = gui->GetFrameHeight();
            for (int option = 0; option < count; ++option)
            {
                gui->SetCursorPos(ImVec2(x + static_cast<float>(option) * segment, y));
                char label[80];
                std::snprintf(label, sizeof(label), "%s##%s %d", TitleCase(options[option]).c_str(), id, option);
                const bool picked  = static_cast<int>(value) == option;
                const bool pressed = WithText(picked ? kText : kMuted, [&] {
                    return Shape(label, ImVec2(segment, frame), picked ? kAccentButton : kRaised, 0.0f);
                });
                Tip(tip);
                if (pressed && !picked)
                {
                    value   = static_cast<Enum>(option);
                    changed = true;
                }
            }
            EndRowAt(x, y, frame);
        }

        template <typename Enum>
        void Segments(const char* label, Enum& value, const char* const* options, int count, const char* tip,
            float segment = 0.0f)
        {
            if (segment <= 0.0f) segment = Em(kSegmentWidth);
            const float width = static_cast<float>(count) * segment;
            const float y     = Row(label, tip, width);
            SegmentsAt(label, value, options, count, segment, tip);
            EndRowAt(cardX, y, gui->GetFrameHeight());
        }

        // Square frames, for sliders, dropdowns and text boxes.
        template <typename Draw>
        auto SquareFrames(Draw draw)
        {
            const float was     = style.FrameRounding;
            style.FrameRounding = Em(kRadius);
            const auto result   = draw();
            style.FrameRounding = was;
            return result;
        }

        // Typed values (Ctrl+click) are clamped with the rest, after the window is drawn.
        void SliderInt(const char* label, int& value, int lo, int hi, const char* format, const char* tip)
        {
            const float y = Row(label, tip, Em(kControlWidth));
            char id[64];
            std::snprintf(id, sizeof(id), "##slider %s", label);
            gui->SetNextItemWidth(Em(kControlWidth));
            SquareFrames([&] { return gui->SliderInt(id, &value, lo, hi, format, ImGuiSliderFlags_AlwaysClamp); });
            changed |= gui->IsItemDeactivatedAfterEdit();
            Tip(tip);
            EndRowAt(cardX, y, gui->GetFrameHeight());
        }

        void SliderFloat(const char* label, float& value, float lo, float hi, const char* format, const char* tip)
        {
            const float y = Row(label, tip, Em(kControlWidth));
            char id[64];
            std::snprintf(id, sizeof(id), "##slider %s", label);
            gui->SetNextItemWidth(Em(kControlWidth));
            SquareFrames([&] { return gui->SliderFloat(id, &value, lo, hi, format, ImGuiSliderFlags_AlwaysClamp); });
            changed |= gui->IsItemDeactivatedAfterEdit();
            Tip(tip);
            EndRowAt(cardX, y, gui->GetFrameHeight());
        }

        // A dropdown whose list is flat rows, the picked one filled: ImGui's selectables are overloaded.
        template <typename Name>
        bool ListPopup(const char* id, const std::string& shown, float width, int count, Name name, int picked,
            int& chosen)
        {
            gui->SetNextItemWidth(width);
            const bool open = SquareFrames([&] { return gui->BeginCombo(id, shown.c_str(), ImGuiComboFlags_HeightLarge); });
            if (!open) return false;
            const ImVec2 align    = style.ButtonTextAlign;
            style.ButtonTextAlign = ImVec2(0.0f, 0.5f);
            bool chose            = false;
            for (int i = 0; i < count; ++i)
            {
                char row[96];
                std::snprintf(row, sizeof(row), "%s##%s %d", name(i).c_str(), id, i);
                if (Shape(row, ImVec2(-FLT_MIN, 0.0f), i == picked ? kPickedPill : kFlat, Em(kRadius)))
                {
                    chosen = i;
                    chose  = true;
                    gui->CloseCurrentPopup();
                }
                if (i == picked) gui->SetItemDefaultFocus();
            }
            style.ButtonTextAlign = align;
            gui->EndCombo();
            return chose;
        }

        template <typename Name>
        void DropdownAt(const char* label, int& value, int first, int last, Name text, float width)
        {
            char id[64];
            std::snprintf(id, sizeof(id), "##dropdown %s", label);
            const int step  = first <= last ? 1 : -1;
            const int count = (last - first) * step + 1;
            int chosen      = 0;
            if (ListPopup(id, text(value), width, count, [&](int i) { return text(first + i * step); },
                    (value - first) * step, chosen) &&
                first + chosen * step != value)
            {
                value   = first + chosen * step;
                changed = true;
            }
        }

        // A dropdown of the whole numbers from first to last, either way round, each shown as text gives it.
        template <typename Name>
        void NumberDropdown(const char* label, int& value, int first, int last, Name text, const char* tip,
            float width = 0.0f)
        {
            if (width <= 0.0f) width = Em(kControlWidth);
            const float y = Row(label, tip, width);
            DropdownAt(label, value, first, last, text, width);
            Tip(tip);
            EndRowAt(cardX, y, gui->GetFrameHeight());
        }

        // A dropdown of fonts between two arrows that step to the font before and after. unset, when given, is a first
        // choice that stands for an empty name; bold, when given, is a checkbox in the row's last frame, where a color
        // pick has its swatch.
        void FontDropdown(const char* label, std::string& font, const std::vector<std::string>& choices,
            const char* tip = nullptr, const char* unset = nullptr, bool* bold = nullptr)
        {
            const float frame = gui->GetFrameHeight();
            const float step  = frame + style.ItemSpacing.x;
            const float list  = Em(kControlWidth) - 2.0f * step - (bold != nullptr ? step : 0.0f);
            const float y     = Row(label, tip, Em(kControlWidth));
            const float x     = gui->GetCursorPosX();
            const int count   = FontEntryCount(choices, unset);
            const int picked  = FontEntry(choices, unset, font);
            int chosen        = picked;
            char id[64];
            const auto arrow = [&](const char* side, ImGuiDir direction, int by) {
                std::snprintf(id, sizeof(id), "##%s font %s", side, label);
                if (SquareFrames([&] { return gui->ArrowButton(id, direction); })) chosen = StepFontEntry(count, picked, by);
            };
            arrow("previous", ImGuiDir_Left, -1);
            gui->SetCursorPos(ImVec2(x + step, y));
            std::snprintf(id, sizeof(id), "##font %s", label);
            int listed = 0;
            if (ListPopup(id, FontShown(choices, unset, font), list, count,
                    [&](int entry) { return FontEntryLabel(choices, unset, entry); }, picked, listed))
                chosen = listed;
            Tip(tip);
            gui->SetCursorPos(ImVec2(x + step + list + style.ItemSpacing.x, y));
            arrow("next", ImGuiDir_Right, 1);
            if (chosen != picked && chosen != kNoFontEntry)
            {
                font    = FontOfEntry(choices, unset, chosen);
                changed = true;
            }
            if (bold != nullptr)
            {
                gui->SetCursorPos(ImVec2(x + 2.0f * step + list + style.ItemSpacing.x, y));
                std::snprintf(id, sizeof(id), "##bold %s", label);
                if (gui->Checkbox(id, bold)) changed = true;
                Tip(kBoldTip);
            }
            EndRowAt(cardX, y, frame);
        }

        void SwatchAt(const char* id, Color& color)
        {
            SquareFrames([&] {
                return gui->ColorEdit3(id, color.v, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
            });
            changed |= gui->IsItemDeactivatedAfterEdit();
        }

        void Swatch(const char* label, Color& color, const char* tip)
        {
            const float y = Row(label, tip, gui->GetFrameHeight());
            char id[64];
            std::snprintf(id, sizeof(id), "##swatch %s", label);
            SwatchAt(id, color);
            Tip(tip);
            EndRowAt(cardX, y, gui->GetFrameHeight());
        }

        // A color pick as a row a slider wide: the dropdown of its source, then the swatch of a custom color in the
        // row's last frame, which stays empty for any other source so the dropdown keeps its place.
        void ColorPickRow(const char* label, ColorPick& pick, const std::vector<ColorSource>& sources, const char* tip)
        {
            const float frame = gui->GetFrameHeight();
            const float list  = Em(kControlWidth) - frame - style.ItemSpacing.x;
            const float y     = Row(label, tip, Em(kControlWidth));
            const float x     = gui->GetCursorPosX();
            char id[64];
            std::snprintf(id, sizeof(id), "##source %s", label);
            const auto at = std::ranges::find(sources, pick.source);
            int chosen    = 0;
            if (ListPopup(id, SourceLabel(pick.source), list, static_cast<int>(sources.size()),
                    [&](int i) { return std::string(SourceLabel(sources[static_cast<size_t>(i)])); },
                    at != sources.end() ? static_cast<int>(at - sources.begin()) : -1, chosen) &&
                sources[static_cast<size_t>(chosen)] != pick.source)
            {
                pick.source = sources[static_cast<size_t>(chosen)];
                changed     = true;
            }
            Tip(tip);
            if (pick.source == ColorSource::Custom)
            {
                gui->SetCursorPos(ImVec2(x + list + style.ItemSpacing.x, y));
                std::snprintf(id, sizeof(id), "##swatch %s", label);
                SwatchAt(id, pick.custom);
            }
            EndRowAt(cardX, y, frame);
        }

        // A row of a list of checks, its dot lit when checked. True when clicked: the list stays open.
        bool CheckRow(const char* label, const char* id, int index, bool checked, float width)
        {
            const float x = gui->GetCursorPosX(), y = gui->GetCursorPosY();
            const float frame = gui->GetFrameHeight(), dot = Em(kChipDot);
            const float padding  = style.FramePadding.x;
            style.FramePadding.x = Em(kCheckIndent);
            char row[96];
            std::snprintf(row, sizeof(row), "%s##%s %d", label, id, index);
            const bool pressed   = Shape(row, ImVec2(width, frame), kFlat, Em(kRadius));
            style.FramePadding.x = padding;
            // Its dot over it, drawn disabled so the row keeps every click.
            gui->BeginDisabled(true);
            gui->SetCursorPos(ImVec2(x + (Em(kCheckIndent) - dot) / 2.0f, y + (frame - dot) / 2.0f));
            std::snprintf(row, sizeof(row), "##dot %s %d", id, index);
            Shape(row, ImVec2(dot, dot), checked ? kAccentButton : ButtonColors{kMuted, kMuted, kMuted}, dot / 2.0f);
            gui->EndDisabled();
            EndRowAt(x, y, frame);
            return pressed;
        }

        // When a line shows, as a dropdown of checks: Always, which checks and locks the rest, then each condition.
        void ChecksDropdown(const char* label, LineShow& show, const std::vector<ShowCondition>& conditions,
            const char* tip)
        {
            const float width = Em(kControlWidth);
            const float y     = Row(label, tip, width);
            char id[64];
            std::snprintf(id, sizeof(id), "##checks %s", label);
            gui->SetNextItemWidth(width);
            const std::string shown = ShowSummary(show, conditions);
            if (SquareFrames([&] { return gui->BeginCombo(id, shown.c_str(), ImGuiComboFlags_HeightLarge); }))
            {
                // Rows as wide as the list, or as its longest entry when that is wider: the list then grows to hold it.
                size_t letters = std::strlen(kShowAlways);
                for (const ShowCondition condition : conditions)
                    letters = std::max(letters, std::strlen(ConditionLabel(condition)));
                const float row = std::max(width - 2.0f * style.FramePadding.x,
                    Em(kCheckIndent) + static_cast<float>(letters) * Em(kLetterWidth) + style.FramePadding.x);
                const ImVec2 align    = style.ButtonTextAlign;
                style.ButtonTextAlign = ImVec2(0.0f, 0.5f);
                if (CheckRow(kShowAlways, id, 0, show.always, row))
                {
                    show.always = !show.always;
                    changed     = true;
                }
                Unless(show.always, [&] {
                    for (size_t i = 0; i < conditions.size(); ++i)
                    {
                        const uint32_t bit = ConditionBit(conditions[i]);
                        if (!CheckRow(ConditionLabel(conditions[i]), id, static_cast<int>(i) + 1,
                                show.always || (show.when & bit) != 0, row))
                            continue;
                        show.when ^= bit;
                        changed = true;
                    }
                    return 0;
                });
                style.ButtonTextAlign = align;
                gui->EndCombo();
            }
            Tip(tip);
            EndRowAt(cardX, y, gui->GetFrameHeight());
        }

        // A strip of equal tabs across the card, a gap apart, the open one filled, rounded at the top and square at the
        // foot, where the last row stands on the rule; perRow to a row when given. Returns the tab clicked, or picked:
        // which tab is open is the window's state, not a setting.
        int Tabs(const char* id, int picked, const char* const* labels, int count, int perRow = 0)
        {
            const int columns  = perRow > 0 ? perRow : count, rows = (count + columns - 1) / columns;
            const float left = cardX + Em(kCardPad), first = gui->GetCursorPosY();
            const float strip  = cardWidth - 2.0f * Em(kCardPad);
            const float height = Em(kTabHeight), corner = Em(kRadius), gap = std::round(Em(kTabGap));
            int chosen         = picked;
            char name[80];
            for (int tab = 0; tab < count; ++tab)
            {
                const TabPlace place       = TabAt(tab, count, columns, strip, height, gap);
                const float x = left + place.x, top = first + place.y, width = place.width;
                const ButtonColors& colors = tab == picked ? kAccentButton : kRaised;
                // A button rounds all four corners: squares in its color lie under the lower two, as grounds, since a
                // disabled item drawn first would take the tab's clicks there.
                for (int side = 0; side < 2; ++side)
                {
                    gui->SetCursorPos(ImVec2(side == 0 ? x : x + width - corner, top + height - corner));
                    std::snprintf(name, sizeof(name), "##%s corner %d %d", id, tab, side);
                    Ground(name, ImVec2(corner, corner), ButtonColors{colors.normal, colors.normal, colors.normal}, 0.0f);
                }
                gui->SetCursorPos(ImVec2(x, top));
                std::snprintf(name, sizeof(name), "%s##%s %d", labels[tab], id, tab);
                // No padding at the sides: ImGui centers a label only in the room its padding leaves, and starts one
                // that does not fit there at the left.
                const float padding  = style.FramePadding.x;
                style.FramePadding.x = 0.0f;
                if (WithText(tab == picked ? kText : kMuted,
                        [&] { return Shape(name, ImVec2(width, height), colors, corner); }))
                    chosen = tab;
                style.FramePadding.x = padding;
            }
            const float tall = static_cast<float>(rows) * height + static_cast<float>(rows - 1) * gap;
            gui->BeginDisabled(true);
            gui->SetCursorPos(ImVec2(left, first + tall));
            std::snprintf(name, sizeof(name), "##%s rule", id);
            Shape(name, ImVec2(strip, Em(kTabRule)), ButtonColors{kAccent, kAccent, kAccent}, 0.0f);
            gui->EndDisabled();
            EndRowAt(cardX, first, tall + Em(kTabRule));
            return chosen;
        }

        // Where the next chip of a wrapping row goes: after the last one, or at left on a new line when room for it
        // would pass the card's right padding.
        void PlaceChip(bool first, float left, float room)
        {
            if (first)
            {
                gui->SetCursorPosX(left);
                return;
            }
            gui->SameLine();
            if (gui->GetCursorPosX() + room <= cardX + cardWidth - Em(kCardPad)) return;
            gui->NewLine();
            gui->SetCursorPosX(left);
        }

        // A tag as a chip that reads as the tag is typed. True when clicked; dragged, it drops onto a line's box.
        bool TagChip(Tag tag)
        {
            const char* name   = TagName(tag);
            const bool pressed = Shape(name, ImVec2(0.0f, 0.0f), kRaised, gui->GetFrameHeight() / 2.0f, kRaisedEdge);
            if (!gui->IsItemActive()) Tip(TagTip(tag));
            if (gui->BeginDragDropSource(0))
            {
                gui->SetDragDropPayload(kTagPayload, &tag, sizeof(tag), 0);
                Text(kText, name);
                gui->EndDragDropSource();
            }
            return pressed;
        }

        // A design line's box across the card, its one line of text in the middle. True when the text changed: typed
        // and then left by Enter or a click elsewhere (Escape takes the typing back), or a tag chip dropped at its end.
        bool LineBox(const char* id, std::string& text, LineDraft& draft)
        {
            if (!draft.typing) std::snprintf(draft.text, sizeof(draft.text), "%s", text.c_str());
            const float line     = gui->GetTextLineHeight();
            const float height   = static_cast<float>(kLineBoxLines) * line + 2.0f * style.FramePadding.y;
            const float padding  = style.FramePadding.y;
            style.FramePadding.y = (height - line) / 2.0f;
            gui->SetNextItemWidth(cardWidth - 2.0f * Em(kCardPad));
            // This ImGui (1.92.3 WIP) cannot wrap a text box, and a box of several lines puts the caret at the text's
            // start or end for a click above or below its one line: a longer design scrolls sideways instead.
            SquareFrames([&] {
                return gui->InputText(id, draft.text, sizeof(draft.text), ImGuiInputTextFlags_CallbackAlways, NoteCaret,
                    &draft.caret);
            });
            style.FramePadding.y = padding;
            draft.typing         = gui->IsItemActive();
            bool edited          = false;
            if (gui->IsItemDeactivatedAfterEdit() && text != draft.text)
            {
                text   = draft.text;
                edited = true;
            }
            if (gui->BeginDragDropTarget())
            {
                if (const ImGuiPayload* payload = gui->AcceptDragDropPayload(kTagPayload, 0);
                    payload != nullptr && payload->DataSize == sizeof(Tag))
                {
                    Tag tag;
                    std::memcpy(&tag, payload->Data, sizeof(tag));
                    size_t end = text.size();
                    if (static_cast<int>(tag) < kTagCount && PutTag(text, end, tag))
                    {
                        draft.caret = end;
                        edited      = true;
                    }
                }
                gui->EndDragDropTarget();
            }
            changed |= edited;
            return edited;
        }

        // A whole number to type, as a pill with its name before it: "X [ 12 ]".
        void NumberAt(const char* name, const char* id, int& value, int limit)
        {
            const float x = gui->GetCursorPosX(), y = gui->GetCursorPosY();
            gui->AlignTextToFramePadding();
            Text(kMuted, name);
            gui->SetCursorPos(ImVec2(x + Em(kNumberName), y));
            gui->SetNextItemWidth(Em(kNumberWidth));
            // Dragged sideways it counts up or down; Ctrl+click or a double click types it.
            SquareFrames([&] {
                return gui->DragInt(id, &value, kDragSpeed, -limit, limit, "%d", ImGuiSliderFlags_AlwaysClamp);
            });
            changed |= gui->IsItemDeactivatedAfterEdit();
            EndRowAt(x, y, gui->GetFrameHeight());
        }

        // An X and a Y to type, side by side.
        void Offset(const char* label, int& x, int& y, int limit, const char* tip)
        {
            const float width = 2.0f * (Em(kNumberName) + Em(kNumberWidth)) + style.ItemSpacing.x;
            const float top   = Row(label, tip, width);
            const float left  = gui->GetCursorPosX();
            char id[64];
            std::snprintf(id, sizeof(id), "##x %s", label);
            NumberAt("X", id, x, limit);
            gui->SetCursorPos(ImVec2(left + Em(kNumberName) + Em(kNumberWidth) + style.ItemSpacing.x, top));
            std::snprintf(id, sizeof(id), "##y %s", label);
            NumberAt("Y", id, y, limit);
            EndRowAt(cardX, top, gui->GetFrameHeight());
        }

        // A muted line inside the card, wrapped at its edge.
        void Note(const char* text)
        {
            gui->SetCursorPosX(cardX + Em(kCardPad));
            gui->PushTextWrapPos(cardX + cardWidth - Em(kCardPad));
            Text(kMuted, text);
            gui->PopTextWrapPos();
        }

        // A pill action inside the card.
        bool Action(const char* label, const char* tip = nullptr)
        {
            gui->SetCursorPosX(cardX + Em(kCardPad));
            const bool pressed = Shape(label, ImVec2(0.0f, 0.0f), kRaised, gui->GetFrameHeight() / 2.0f, kRaisedEdge);
            Tip(tip);
            return pressed;
        }

        // The last item takes a dragged chip, which goes before position at of section to.
        void DropTarget(BarSection to, size_t at, std::optional<Drop>& drop)
        {
            if (!gui->BeginDragDropTarget()) return;
            if (const ImGuiPayload* payload = gui->AcceptDragDropPayload(kWidgetPayload, 0);
                payload != nullptr && payload->DataSize == sizeof(DraggedWidget))
            {
                DraggedWidget dragged;
                std::memcpy(&dragged, payload->Data, sizeof(dragged));
                drop = Drop{static_cast<BarSection>(dragged.section), dragged.index, to, at};
            }
            gui->EndDragDropTarget();
        }
    };
}
