#include "menu.h"

#include "Ashita.h"

#include <cfloat>
#include <cstdio>
#include <initializer_list>
#include <utility>

namespace headsup
{
    namespace
    {
        constexpr ImVec4 Hex(uint32_t rgb, float alpha = 1.0f)
        {
            return ImVec4(static_cast<float>((rgb >> 16) & 0xFF) / 255.0f, static_cast<float>((rgb >> 8) & 0xFF) / 255.0f,
                static_cast<float>(rgb & 0xFF) / 255.0f, alpha);
        }

        // The Phoenix palette of Cadence's settings window (github.com/KiplingFFXI/cadence, cadence/ui/theme.lua), the
        // colors of phoenix-xi.com.
        constexpr ImVec4 kBackground   = Hex(0x180E0E, 0.96f);
        constexpr ImVec4 kCard         = Hex(0x291C1C);
        constexpr ImVec4 kControl      = Hex(0x321F1F);
        constexpr ImVec4 kHover        = Hex(0x3A2525);
        constexpr ImVec4 kBorder       = Hex(0xD2ABAB, 0.20f);
        constexpr ImVec4 kText         = Hex(0xFFF8F8);
        constexpr ImVec4 kHeading      = Hex(0xD2ABAB);
        constexpr ImVec4 kMuted        = Hex(0x8A6B6B);
        constexpr ImVec4 kAccent       = Hex(kPhoenixRed);
        constexpr ImVec4 kAccentHover  = Hex(0xFF8D79);
        constexpr ImVec4 kDivider      = Hex(0xD2ABAB, 0.12f);
        constexpr ImVec4 kTrack        = Hex(0x180E0E);
        constexpr ImVec4 kGrabHeld     = Hex(0xD45E5E);
        constexpr ImVec4 kTint         = Hex(kPhoenixRed, 0.30f); // a box being clicked
        constexpr ImVec4 kPicked       = Hex(0xC55151, 0.25f); // the selected page and tab
        constexpr ImVec4 kSelection    = Hex(0xC55151, 0.35f);
        constexpr ImVec4 kRowPicked    = Hex(0xC55151, 0.45f);
        constexpr ImVec4 kRowHovered   = Hex(0xC55151, 0.70f);
        constexpr ImVec4 kClear        = Hex(0x000000, 0.0f);

        struct ButtonColors
        {
            ImVec4 normal, hovered, active;
        };
        constexpr ButtonColors kFlatButton{kClear, kHover, kTint};
        constexpr ButtonColors kCardButton{kCard, kHover, kTint};
        constexpr ButtonColors kControlButton{kControl, kHover, kTint};
        constexpr ButtonColors kRowButton{kRowPicked, kRowHovered, kTint};
        constexpr ButtonColors kPickedButton{kPicked, kPicked, kPicked};
        constexpr ButtonColors kAccentBar{kAccent, kAccent, kAccent};
        constexpr ButtonColors kNoBar{kClear, kClear, kClear};

        // XIUI's config window layout: a sidebar of pages, each with settings and color settings tabs.
        constexpr float kSidebarWidth  = 170.0f;
        constexpr float kSidebarButtonHeight = 32.0f;
        constexpr float kSidebarSpacing      = 2.0f;
        constexpr float kAccentWidth   = 3.0f;   // the bar beside the selected page and under the selected tab
        constexpr float kContentWidth  = 440.0f;
        constexpr float kTabWidth      = 140.0f;
        constexpr float kTabHeight     = 28.0f;
        constexpr float kControlWidth  = 200.0f; // sliders
        constexpr float kNumberWidth   = 56.0f;  // the box to type a slider's value in
        constexpr float kTipWidth      = 300.0f;
        constexpr float kChipWidth     = 52.0f;
        constexpr float kChoiceColumn  = 200.0f; // where a choice's buttons start in its row
        constexpr float kChoiceWidth   = 64.0f;
        constexpr float kOffAlpha      = 0.45f;  // settings of a feature that is off are drawn this faint
        // One size for every page, the tallest included; a longer page scrolls.
        constexpr float kWindowWidth   = 700.0f;
        constexpr float kWindowHeight  = 560.0f;
        // XIUI's corners and spacing.
        constexpr float kWindowRounding = 6.0f;
        constexpr float kRounding       = 4.0f;
        constexpr float kBorderSize     = 1.0f; // the window's, and a chip's
        constexpr ImVec2 kWindowPadding = ImVec2(14.0f, 12.0f);
        constexpr ImVec2 kFramePadding  = ImVec2(8.0f, 4.0f);
        constexpr ImVec2 kItemSpacing   = ImVec2(8.0f, 7.0f);
        constexpr ImVec2 kCellPadding   = ImVec2(12.0f, 0.0f);
        constexpr float kGrabMinSize    = 10.0f;

        enum Page : int
        {
            kOutlinesPage,
            kNameplatesPage,
            kDebugPage,
            kPageCount,
        };
        const char* const kPageLabels[kPageCount] = {"Outlines", "Nameplates", "Debug"};

        // One bit each in Menu::m_Collapsed.
        enum Section : uint32_t
        {
            kOutlineSection         = 1u << 0,
            kMobsSection            = 1u << 1,
            kMobColorsSection       = 1u << 2,
            kNamesSection           = 1u << 3,
            kTextSection            = 1u << 4,
            kTextColorsSection      = 1u << 5,
            kConColorsSection       = 1u << 6,
            kDebugOutlinesSection   = 1u << 7,
            kDebugNameplatesSection = 1u << 8,
            kDebugFrameSection      = 1u << 9,
            kPlayerIconsSection     = 1u << 10,
            kMobInfoSection         = 1u << 11,
            kHideSection            = 1u << 12,
            kPlaceholdersSection    = 1u << 13,
            kCursorSection          = 1u << 14,
            kCursorColorsSection    = 1u << 15,
        };

        const char* const kCategoryLabels[kCategoryCount] = {"Aggressive", "Passive", "No data", "Aggressive NM",
            "Passive NM", "NM placeholder"};
        const char* const kPlayerIconLabels[kPlayerIconCount] = {"GM", "Mentor", "New adventurer", "Level sync", "Away",
            "Linkshell", "Bazaar", "Seeking party"};
        const char* const kIconSideLabels[kIconSideCount] = {"Left", "Right", "Hide"};
        const char* const kMobIdLabels[kMobIdFormatCount]  = {"Off", "Last 3", "Full"};
        const char* const kPlayerIconTips[kPlayerIconCount] = {"A game master.", "A mentor.", "A new adventurer.",
            "Level synced, outside battlefields.", "Away from the keyboard.", "In a linkshell, in the linkshell's color.",
            "Has a bazaar set up.", "Seeking a party."};
        const char* const kCategoryTips[kCategoryCount] = {
            "Aggressive mobs that would attack you at your level: not Too Weak, unless you are sitting or resting.",
            "Mobs that would leave you alone: passive, or aggressive but Too Weak for you.",
            "Mobs the Phoenix data does not list, so HeadsUp cannot tell.",
            "Notorious monsters that would attack you.",
            "Notorious monsters that would leave you alone.",
            "Mobs whose death can pop a notorious monster (lottery placeholders), whether or not they attack. Off, they "
            "are colored like any mob."};
        // Ordinary mobs, notorious monsters and their placeholders, then mobs with no data.
        const Category kCategoryOrder[kCategoryCount] = {Category::WillAttack, Category::WontAttack, Category::NmWillAttack,
            Category::NmWontAttack, Category::Placeholder, Category::Unknown};
        const char* const kShadeLabels[kLabelShadeCount] = {"Unknown level", "Too Weak", "Easy Prey", "Decent Challenge",
            "Even Match", "Tough", "Very Tough"};

        void ApplyPhoenixStyle(ImGuiStyle& s)
        {
            ImVec4* c                          = s.Colors;
            c[ImGuiCol_Text]                   = kText;
            c[ImGuiCol_TextDisabled]           = kMuted;
            c[ImGuiCol_TextSelectedBg]         = kSelection;
            c[ImGuiCol_InputTextCursor]        = kText;
            c[ImGuiCol_WindowBg]               = kBackground;
            c[ImGuiCol_ChildBg]                = kClear;
            c[ImGuiCol_PopupBg]                = kCard;
            c[ImGuiCol_Border]                 = kBorder;
            c[ImGuiCol_TitleBg]                = kCard;
            c[ImGuiCol_TitleBgActive]          = kCard;
            c[ImGuiCol_TitleBgCollapsed]       = kCard;
            c[ImGuiCol_FrameBg]                = kControl;
            c[ImGuiCol_FrameBgHovered]         = kHover;
            c[ImGuiCol_FrameBgActive]          = kTint;
            c[ImGuiCol_CheckMark]              = kAccent;
            c[ImGuiCol_SliderGrab]             = kAccent;
            c[ImGuiCol_SliderGrabActive]       = kGrabHeld;
            c[ImGuiCol_Button]                 = kControl;
            c[ImGuiCol_ButtonHovered]          = kHover;
            c[ImGuiCol_ButtonActive]           = kAccent;
            c[ImGuiCol_Header]                 = kRowPicked;
            c[ImGuiCol_HeaderHovered]          = kRowHovered;
            c[ImGuiCol_HeaderActive]           = kAccent;
            c[ImGuiCol_Separator]              = kDivider;
            c[ImGuiCol_SeparatorHovered]       = kAccentHover;
            c[ImGuiCol_SeparatorActive]        = kAccent;
            c[ImGuiCol_ResizeGrip]             = kHover;
            c[ImGuiCol_ResizeGripHovered]      = kTint;
            c[ImGuiCol_ResizeGripActive]       = kAccent;
            c[ImGuiCol_ScrollbarBg]            = kTrack;
            c[ImGuiCol_ScrollbarGrab]          = kControl;
            c[ImGuiCol_ScrollbarGrabHovered]   = kHover;
            c[ImGuiCol_ScrollbarGrabActive]    = kAccent;
            c[ImGuiCol_TableBorderLight]       = kDivider;
            c[ImGuiCol_TableBorderStrong]      = kBorder;
            c[ImGuiCol_DragDropTarget]         = kAccent;
            s.WindowRounding = kWindowRounding;
            s.ChildRounding = s.FrameRounding = s.PopupRounding = s.GrabRounding = s.TabRounding = s.ScrollbarRounding =
                kRounding;
            s.WindowBorderSize = kBorderSize;
            s.FrameBorderSize  = 0.0f;
            s.WindowPadding    = kWindowPadding;
            s.FramePadding     = kFramePadding;
            s.ItemSpacing      = kItemSpacing;
            s.CellPadding      = kCellPadding;
            s.GrabMinSize      = kGrabMinSize;
        }

        // PushStyleColor and PushStyleVar are overloaded, so colors and sizes are set in the style itself while an item
        // draws (tools/abi_check.py).
        struct Ui
        {
            IGuiManager* gui;
            ImGuiStyle& style;
            float alpha; // the window's own, before fading
            bool changed = false;

            void Colored(const ImVec4& color, const char* text)
            {
                ImVec4& slot     = style.Colors[ImGuiCol_Text];
                const ImVec4 was = slot;
                slot             = color;
                gui->TextUnformatted(text);
                slot = was;
            }

            template <typename Draw>
            bool WithButtonColors(const ButtonColors& colors, Draw draw)
            {
                ImVec4* c           = style.Colors;
                const ImVec4 was[3] = {c[ImGuiCol_Button], c[ImGuiCol_ButtonHovered], c[ImGuiCol_ButtonActive]};
                c[ImGuiCol_Button]        = colors.normal;
                c[ImGuiCol_ButtonHovered] = colors.hovered;
                c[ImGuiCol_ButtonActive]  = colors.active;
                const bool pressed        = draw();
                c[ImGuiCol_Button]        = was[0];
                c[ImGuiCol_ButtonHovered] = was[1];
                c[ImGuiCol_ButtonActive]  = was[2];
                return pressed;
            }

            bool Button(const char* label, const ImVec2& size, const ButtonColors& colors)
            {
                return WithButtonColors(colors, [&] { return gui->Button(label, size); });
            }

            // Each font is a flat full-width button: ImGui's selectables are overloaded.
            void FontDropdown(std::string& font, const std::vector<std::string>& choices)
            {
                gui->SetNextItemWidth(kControlWidth);
                if (!gui->BeginCombo("Font", font.c_str(), ImGuiComboFlags_HeightLarge)) return;
                const ImVec2 align    = style.ButtonTextAlign;
                style.ButtonTextAlign = ImVec2(0.0f, 0.5f);
                for (const std::string& name : choices)
                {
                    const bool picked = name == font;
                    if (Button(name.c_str(), ImVec2(-FLT_MIN, 0.0f), ButtonColors{picked ? kRowPicked : kClear, kRowHovered, kTint}))
                    {
                        font    = name;
                        changed = true;
                        gui->CloseCurrentPopup();
                    }
                    if (picked) gui->SetItemDefaultFocus();
                }
                style.ButtonTextAlign = align;
                gui->EndCombo();
            }

            // A (?) after the last item that explains the setting, even while the setting is faded.
            void Help(const char* tip)
            {
                gui->SameLine();
                Colored(kMuted, "(?)");
                Tip(tip);
            }

            void Tip(const char* tip)
            {
                if (!gui->IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) return;
                const float faded = style.Alpha;
                style.Alpha       = alpha;
                gui->BeginTooltip();
                gui->PushTextWrapPos(kTipWidth);
                gui->TextUnformatted(tip);
                gui->PopTextWrapPos();
                gui->EndTooltip();
                style.Alpha = faded;
            }

            // A full-width header with an arrow, like XIUI's collapsing sections (CollapsingHeader is overloaded).
            bool Section(const char* title, uint32_t bit, uint32_t& collapsed)
            {
                gui->Spacing();
                const bool open = (collapsed & bit) == 0;
                // Headers share the window with the sidebar, whose pages can have the same names.
                char arrow[48], label[48];
                std::snprintf(arrow, sizeof(arrow), "##section arrow %s", title);
                std::snprintf(label, sizeof(label), "%s##section", title);
                bool toggled = WithButtonColors(kCardButton, [&] { return gui->ArrowButton(arrow, open ? ImGuiDir_Down : ImGuiDir_Right); });
                gui->SameLine(0.0f, 0.0f);
                ImVec4* c             = style.Colors;
                const ImVec2 align    = style.ButtonTextAlign;
                const ImVec4 text     = c[ImGuiCol_Text];
                style.ButtonTextAlign = ImVec2(0.0f, 0.5f);
                c[ImGuiCol_Text]      = kHeading;
                toggled |= Button(label, ImVec2(kContentWidth - gui->GetFrameHeight(), 0.0f), kCardButton);
                c[ImGuiCol_Text]      = text;
                style.ButtonTextAlign = align;
                if (toggled) collapsed ^= bit;
                if (open) gui->Spacing();
                return open;
            }

            // Faint while off, but still usable, so a feature can be set up before turning it on. Not while a color
            // picker is open, which would be faint too.
            void Fade(bool off)
            {
                style.Alpha = off && !gui->IsPopupOpen("", ImGuiPopupFlags_AnyPopupId) ? alpha * kOffAlpha : alpha;
            }

            void Check(const char* label, bool& value, const char* tip)
            {
                changed |= gui->Checkbox(label, &value);
                Help(tip);
            }

            template <typename Enum>
            void Choice(const char* label, Enum& value, const char* const* options, int count, const char* tip)
            {
                const float rowStart = gui->GetCursorPosX();
                gui->AlignTextToFramePadding();
                gui->TextUnformatted(label);
                Help(tip);
                for (int option = 0; option < count; ++option)
                {
                    gui->SameLine();
                    if (option == 0) gui->SetCursorPosX(rowStart + kChoiceColumn);
                    char id[48];
                    std::snprintf(id, sizeof(id), "%s##choice %s", options[option], label);
                    const bool picked         = static_cast<int>(value) == option;
                    if (Button(id, ImVec2(kChoiceWidth, 0.0f), picked ? kRowButton : kCardButton) && !picked)
                    {
                        value   = static_cast<Enum>(option);
                        changed = true;
                    }
                }
            }

            // Typed values are clamped with the rest, after the window is drawn.
            void SliderInt(const char* label, int& value, int lo, int hi, const char* format, const char* tip)
            {
                char id[48];
                std::snprintf(id, sizeof(id), "##%s", label);
                gui->SetNextItemWidth(kControlWidth);
                gui->SliderInt(id, &value, lo, hi, format, ImGuiSliderFlags_AlwaysClamp);
                changed |= gui->IsItemDeactivatedAfterEdit();
                gui->SameLine();
                gui->SetNextItemWidth(kNumberWidth);
                gui->InputInt(label, &value, 0, 0, 0);
                changed |= gui->IsItemDeactivatedAfterEdit();
                Help(tip);
            }

            void SliderFloat(const char* label, float& value, float lo, float hi, const char* format, const char* number,
                const char* tip)
            {
                char id[48];
                std::snprintf(id, sizeof(id), "##%s", label);
                gui->SetNextItemWidth(kControlWidth);
                gui->SliderFloat(id, &value, lo, hi, format, ImGuiSliderFlags_AlwaysClamp);
                changed |= gui->IsItemDeactivatedAfterEdit();
                gui->SameLine();
                gui->SetNextItemWidth(kNumberWidth);
                gui->InputFloat(label, &value, 0.0f, 0.0f, number, 0);
                changed |= gui->IsItemDeactivatedAfterEdit();
                Help(tip);
            }

            void Swatch(const char* label, Color& color, const char* tip)
            {
                gui->ColorEdit3(label, color.v, ImGuiColorEditFlags_NoInputs);
                changed |= gui->IsItemDeactivatedAfterEdit();
                Help(tip);
            }

            // An ON/OFF chip, filled when on and outlined when off, like the filter chips on phoenix-xi.com.
            void Chip(bool& on)
            {
                ImVec4* c           = style.Colors;
                const ImVec4 was[2] = {c[ImGuiCol_Text], c[ImGuiCol_Border]};
                const float border  = style.FrameBorderSize;
                c[ImGuiCol_Text]      = on ? kText : kHeading;
                c[ImGuiCol_Border]    = on ? kAccent : kBorder;
                style.FrameBorderSize = kBorderSize;
                char label[32];
                std::snprintf(label, sizeof(label), "%s##enabled", on ? "ON" : "OFF");
                if (Button(label, ImVec2(kChipWidth, 0.0f), on ? ButtonColors{kAccent, kAccentHover, kAccent} : kFlatButton))
                {
                    on      = !on;
                    changed = true;
                }
                style.FrameBorderSize = border;
                c[ImGuiCol_Text]      = was[0];
                c[ImGuiCol_Border]    = was[1];
            }
        };

        // The window's own title row, in place of ImGui's title bar so the ON/OFF chip can sit beside the name. Dragging
        // any empty part of the window moves it.
        void DrawTitle(Ui& ui, Settings& s, bool& open, double version)
        {
            ui.gui->AlignTextToFramePadding();
            ui.Colored(kText, "HeadsUp");
            ui.gui->SameLine();
            char shown[16];
            std::snprintf(shown, sizeof(shown), "v%g", version);
            ui.Colored(kMuted, shown);
            ui.gui->SameLine();
            ui.Chip(s.enabled);
            ui.Tip("Turns every outline and nameplate on or off, the same as /hu on and /hu off.");
            ui.gui->SameLine();
            const float size = ui.gui->GetFrameHeight();
            ui.gui->SetCursorPosX(ui.gui->GetWindowWidth() - ui.style.WindowPadding.x - size);
            if (ui.Button("x##close", ImVec2(size, size), kFlatButton)) open = false;
            ui.Tip("Close. /hu opens it again.");
            ui.gui->Spacing();
        }

        void DrawSidebar(Ui& ui, int& page)
        {
            const ImVec2 spacing = ui.style.ItemSpacing;
            ui.style.ItemSpacing = ImVec2(0.0f, kSidebarSpacing);
            for (int p = 0; p < kPageCount; ++p)
            {
                const bool picked = p == page;
                char bar[16], label[32];
                std::snprintf(bar, sizeof(bar), "##bar%d", p);
                std::snprintf(label, sizeof(label), "%s##page", kPageLabels[p]);
                bool pressed = ui.Button(bar, ImVec2(kAccentWidth, kSidebarButtonHeight), picked ? kAccentBar : kNoBar);
                ui.gui->SameLine();
                pressed |= ui.Button(label, ImVec2(kSidebarWidth - kAccentWidth, kSidebarButtonHeight), picked ? kPickedButton : kFlatButton);
                if (pressed) page = p;
            }
            ui.style.ItemSpacing = spacing;
        }

        void DrawTabs(Ui& ui, bool& colorsTab, bool hasColors)
        {
            const char* const labels[2] = {"Settings", "Color Settings"};
            const int tabs              = hasColors ? 2 : 1;
            // No gap under the tabs: a row's gap is taken when its last item is placed.
            const float spacingY   = ui.style.ItemSpacing.y;
            ui.style.ItemSpacing.y = 0.0f;
            for (int t = 0; t < tabs; ++t)
            {
                const bool picked = (t == 1) == colorsTab;
                if (t > 0) ui.gui->SameLine();
                if (ui.Button(labels[t], ImVec2(kTabWidth, kTabHeight), picked ? kPickedButton : kControlButton)) colorsTab = t == 1;
            }
            for (int t = 0; t < tabs; ++t)
            {
                const bool picked = (t == 1) == colorsTab;
                char line[16];
                std::snprintf(line, sizeof(line), "##line%d", t);
                if (t > 0) ui.gui->SameLine();
                ui.Button(line, ImVec2(kTabWidth, kAccentWidth), picked ? kAccentBar : kNoBar);
            }
            ui.style.ItemSpacing.y = spacingY;
            ui.gui->Spacing();
            ui.gui->Separator();
        }

        void DrawOutlineSettings(Ui& ui, Settings& s, uint32_t& collapsed, const MenuStatus& status)
        {
            ui.Fade(!s.enabled);
            if (ui.Section("Outline", kOutlineSection, collapsed))
            {
                ui.SliderFloat("Thickness", s.thickness, kMinThickness, kMaxThickness, "%.1f px", "%.1f",
                    "How wide the outline is, in pixels.");
                ui.SliderInt("Smoothness", s.smoothness, kMinSmoothness, kMaxSmoothness, "%d copies",
                    "How many shifted copies draw the outline: more is rounder and costs a little more.");
                ui.SliderFloat("Max distance", s.maxDistance, kMinOutlineDistance, kMaxOutlineDistance, "%.0f yalms", "%.0f",
                    "Mobs farther away, in yalms, get no outline. Nameplates are not limited.");
            }
            if (ui.Section("Mobs to outline", kMobsSection, collapsed))
            {
                for (const Category category : kCategoryOrder)
                {
                    const int c = CategoryIndex(category);
                    ui.Check(kCategoryLabels[c], s.show[c], kCategoryTips[c]);
                }
            }
            ui.Fade(false);
            if (!status.stencilAvailable) ui.Colored(kAccent, "Outlines are unavailable: the depth buffer has no stencil bits.");
        }

        void DrawOutlineColors(Ui& ui, Settings& s, uint32_t& collapsed)
        {
            ui.Fade(!s.enabled);
            if (ui.Section("Mob colors", kMobColorsSection, collapsed))
            {
                for (const Category category : kCategoryOrder)
                {
                    const int c = CategoryIndex(category);
                    ui.Fade(!s.enabled || !s.show[c]);
                    ui.Swatch(kCategoryLabels[c], s.color[c], kCategoryTips[c]);
                }
            }
            ui.Fade(false);
        }

        void DrawNameplateSettings(Ui& ui, Settings& s, uint32_t& collapsed, const std::vector<std::string>& fonts)
        {
            const bool anyNames = ReplacesAnyName(s);
            ui.Fade(!s.enabled);
            if (ui.Section("Names", kNamesSection, collapsed))
            {
                ui.Check("Replace mob names", s.replaceMobNames,
                    "Hides the game's mob names and draws them in your font, in the game's color.");
                ui.Check("Replace player names", s.replacePlayerNames,
                    "The same for players, you included. Levels and MobDB icons are for mobs only.");
                ui.Check("Replace NPC names", s.replaceNpcNames, "The same for NPCs.");
            }
            if (ui.Section("Text", kTextSection, collapsed))
            {
                ui.FontDropdown(s.fontName, fonts);
                ui.gui->SameLine();
                ui.Check("Bold", s.fontBold, "Bold names and level text.");
                ui.Fade(!anyNames);
                ui.SliderInt("Name size", s.nameSize, kMinTextSize, kMaxTextSize, "%d px",
                    "The height of the names HeadsUp draws.");
                ui.SliderInt("Raise names", s.nameRaise, kMinNameRaise, kMaxNameRaise, "%d px",
                    "How far above the game's own spot the names HeadsUp draws sit, with everything above them.");
                ui.Fade(!s.enabled);
                ui.Check("Scale with distance", s.scaleWithDistance,
                    "Every size grows and shrinks with the game's own name size as the name comes closer or moves away.");
            }
            if (ui.Section("Level and icons", kMobInfoSection, collapsed))
            {
                ui.Check("Show level and con", s.showLabels,
                    "Lv 20-23 EP-DC: the level range from the Phoenix data and how it cons to you. After you /check the "
                    "mob, its exact level, until it respawns.");
                ui.Fade(!s.enabled || !s.showLabels);
                ui.SliderInt("Level and con size", s.labelSize, kMinTextSize, kMaxTextSize, "%d px", "The level line's height.");
                ui.Fade(!s.enabled);
                ui.Check("Show icons", s.showIcons,
                    "XIUI's MobDB icons: aggressive or passive, whether it links, and how it detects you.");
                ui.Fade(!s.enabled || !s.showIcons);
                ui.SliderInt("Icon size", s.iconSize, kMinTextSize, kMaxTextSize, "%d px", "Each MobDB icon's width and height.");
                ui.Fade(!s.enabled);
                ui.Choice("Mob ID", s.mobId, kMobIdLabels, kMobIdFormatCount,
                    "The mob's ID on its level line: its last three hex digits (Lv 1-3 EM [006]), as players name NM "
                    "placeholders, or the whole ID in decimal and hex.");
            }
            ui.Fade(!s.enabled || (!s.showLabels && s.mobId == MobIdFormat::Off && !s.showIcons));
            if (ui.Section("Hide level and icons", kHideSection, collapsed))
            {
                ui.Check("On mobs your party claimed", s.hideClaimedByParty,
                    "Takes the level line and icons off a mob once you or your party has claimed it. Its name and the "
                    "cursor stay.");
                ui.Check("On mobs anyone claimed", s.hideClaimed, "Takes the level line and icons off any mob someone has claimed.");
                ui.Check("On Too Weak mobs", s.hideTooWeak, "Takes the level line and icons off mobs that con Too Weak to you.");
                ui.Check("On every mob while you fight", s.hideWhileEngaged,
                    "Takes the level line and icons off every mob while you are engaged.");
            }
            ui.Fade(!s.enabled);
            if (ui.Section("NM placeholders", kPlaceholdersSection, collapsed))
            {
                ui.Fade(!s.enabled || s.mobId == MobIdFormat::Off);
                ui.Check("Mark them [PH]", s.markPlaceholders,
                    "[PH] in place of the last three digits of the Mob ID (after the whole ID) on a mob whose death can "
                    "pop a notorious monster, from the Phoenix data.");
                ui.Fade(!s.enabled);
                ui.Check("Respawn timers", s.phTimers,
                    "Above your name, a respawn countdown for each NM placeholder you see die on screen: the NM, the "
                    "placeholder's last three hex digits and the time left (Bigmouth Billy [11A] 3:15). Phoenix starts it "
                    "15 seconds after the death, when the body despawns. It reads \"up\" when due, until you see the "
                    "placeholder alive or five minutes pass.");
                ui.Fade(!s.enabled || !s.phTimers);
                ui.SliderInt("Timer size", s.timerSize, kMinTextSize, kMaxTextSize, "%d px", "The timer lines' height.");
            }
            ui.Fade(!s.enabled || !s.replacePlayerNames);
            if (ui.Section("Player icons", kPlayerIconsSection, collapsed))
            {
                ui.Check("Show player icons", s.showPlayerIcons,
                    "Beside replaced player names: seeking party, bazaar, linkshell in its color, away, mentor, new "
                    "adventurer, GM and level sync, in XIUI's HQ versions of the game's icons. Each can go left or right of "
                    "the name, or be hidden.");
                ui.Fade(!s.enabled || !s.replacePlayerNames || !s.showPlayerIcons);
                ui.Check("Center name and icons", s.centerNameAndIcons,
                    "Centers a player's name and the icons beside it together over them, as the game does. Off, the name "
                    "alone is centered and the icons hang to its sides.");
                ui.SliderInt("Player icon size", s.playerIconSize, kMinPlayerIconSize, kMaxPlayerIconSize, "%d%%",
                    "The icons beside player names, as a share of the name's height, so they grow and shrink with it.");
                for (int i = 0; i < kPlayerIconCount; ++i)
                    ui.Choice(kPlayerIconLabels[i], s.playerIconSide[i], kIconSideLabels, kIconSideCount, kPlayerIconTips[i]);
            }
            ui.Fade(!s.enabled);
            if (ui.Section("Cursor and pointer", kCursorSection, collapsed))
            {
                ui.Check("Replace target cursor", s.replaceCursor,
                    "Hides the game's cursor over your target and draws HeadsUp's above its nameplate instead: one color for "
                    "your target, one while you are locked on, and one for the sub-target cursor.");
                ui.Fade(!s.enabled || !s.replaceCursor);
                ui.Check("Phoenix feather cursor", s.cursorFeather, "Phoenix's feather icon instead of the arrow, in the same colors.");
                ui.SliderInt("Cursor size", s.cursorSize, kMinTextSize, kMaxTextSize, "%d px", "The target cursor's height.");
                ui.Fade(!s.enabled);
                ui.Check("Chocobo mouse pointer", s.chocoboPointer,
                    "PlayOnline's chocobo in place of the game's mouse pointer, over menus too. The camera arrows stay the "
                    "game's.");
            }
            ui.Fade(false);
        }

        void DrawNameplateColors(Ui& ui, Settings& s, uint32_t& collapsed)
        {
            ui.Fade(!s.enabled);
            if (ui.Section("Text and icons", kTextColorsSection, collapsed))
            {
                const bool anyNames = ReplacesAnyName(s);
                ui.Fade(!anyNames);
                ui.changed |= ui.gui->Checkbox("##ownNameColor", &s.ownNameColor);
                ui.gui->SameLine();
                ui.Fade(!anyNames || !s.ownNameColor);
                ui.Swatch("Own name color", s.nameColor,
                    "Off: names keep the game's color, which shows who claimed the mob. On: every name takes this color.");
                ui.Fade(!s.enabled);
                ui.Swatch("Text outline", s.textOutline, "The edge around the name and the level text.");
                ui.Fade(!s.enabled || !s.showIcons);
                ui.Swatch("Icon tint", s.iconTint, "Multiplies the MobDB icons' colors. White keeps them as they are.");
            }
            ui.Fade(!s.enabled || !s.showLabels);
            if (ui.Section("Level and con", kConColorsSection, collapsed))
            {
                ui.Colored(kMuted, "One color for each con.");
                ui.Help("Easy Prey also covers Incredibly Easy Prey, and Very Tough covers Incredibly Tough. Unknown level "
                        "is used until the mob's level or your own is known.");
                // A table sizes its columns to the widest name; measuring text returns a struct (tools/abi_check.py).
                if (ui.gui->BeginTable("##cons", 2, ImGuiTableFlags_SizingFixedFit))
                {
                    for (int k = 0; k < kLabelShadeCount; ++k)
                    {
                        ui.gui->TableNextColumn();
                        ui.gui->ColorEdit3(kShadeLabels[k], s.labelColor[k].v, ImGuiColorEditFlags_NoInputs);
                        ui.changed |= ui.gui->IsItemDeactivatedAfterEdit();
                    }
                    ui.gui->EndTable();
                }
            }
            ui.Fade(!s.enabled || !s.replaceCursor);
            if (ui.Section("Target cursor", kCursorColorsSection, collapsed))
            {
                ui.Swatch("Target cursor", s.cursorColor, "The cursor over your target.");
                ui.Swatch("Locked-on cursor", s.lockedCursorColor, "The cursor over your target while you are locked on.");
                ui.Swatch("Sub-target cursor", s.subCursorColor,
                    "The cursor over what you are picking for a spell, an ability or a trade.");
                ui.Swatch("Out of range cursor", s.outOfRangeCursorColor,
                    "The cursor over what you are picking while it is out of range of the spell or ability, as the game's "
                    "own cursor turns red.");
            }
            ui.Fade(false);
        }

        void DebugRows(Ui& ui, const char* id, std::initializer_list<std::pair<const char*, const char*>> rows)
        {
            if (!ui.gui->BeginTable(id, 2, ImGuiTableFlags_SizingFixedFit)) return;
            for (const auto& [label, value] : rows)
            {
                ui.gui->TableNextColumn();
                ui.Colored(kMuted, label);
                ui.gui->TableNextColumn();
                ui.gui->TextUnformatted(value);
            }
            ui.gui->EndTable();
        }

        void DrawDebug(Ui& ui, const MenuStatus& status, uint32_t& collapsed, bool& requested)
        {
            char outlined[32], meshes[32], shown[32], names[64], frame[48], level[32];
            if (ui.Section("Outlines", kDebugOutlinesSection, collapsed))
            {
                std::snprintf(outlined, sizeof(outlined), "%u", status.outlinedMobs);
                std::snprintf(meshes, sizeof(meshes), "%u", status.meshes);
                DebugRows(ui, "##debugOutlines", {{"Mobs outlined", outlined}, {"Meshes outlined last frame", meshes},
                    {"Stencil buffer", status.stencilAvailable ? "available" : "missing: no outlines"}});
            }
            if (ui.Section("Nameplates", kDebugNameplatesSection, collapsed))
            {
                std::snprintf(shown, sizeof(shown), "%u", status.nameplates);
                if (status.hooked)
                    std::snprintf(names, sizeof(names), "%u this frame, %u drawn by HeadsUp", status.names.names,
                        status.names.replaced);
                else
                    std::snprintf(names, sizeof(names), "not hooked");
                DebugRows(ui, "##debugNameplates", {{"Nameplates shown", shown},
                    {"Drawn", status.drewInScene ? "into the scene, behind walls" : "on top"}, {"Game's names", names}});
            }
            if (ui.Section("Frame", kDebugFrameSection, collapsed))
            {
                std::snprintf(frame, sizeof(frame), "%.2f ms (%.0f fps)", status.frameMs,
                    status.frameMs > 0.0 ? 1000.0 / status.frameMs : 0.0);
                if (status.player.level > 0)
                    std::snprintf(level, sizeof(level), "%d%s", status.player.level, status.player.sitting ? ", sitting" : "");
                else
                    std::snprintf(level, sizeof(level), "unknown");
                DebugRows(ui, "##debugFrame", {{"Frame time", frame}, {"Your level", level}});
            }
            ui.gui->Spacing();
            if (ui.Button("Write debug report", ImVec2(0.0f, 0.0f), kControlButton)) requested = true;
            ui.Help("The same as /hu debug: writes what every outline and nameplate show to logs/headsup, then records "
                    "the next frames' nameplate data.");
        }

        int CALLBACK AddFamily(const LOGFONTA* font, const TEXTMETRICA* metrics, DWORD type, LPARAM families)
        {
            (void)metrics;
            if ((type & TRUETYPE_FONTTYPE) != 0) reinterpret_cast<std::vector<std::string>*>(families)->push_back(font->lfFaceName);
            return 1;
        }

        std::vector<std::string> InstalledFontFamilies()
        {
            std::vector<std::string> families;
            HDC dc = CreateCompatibleDC(nullptr);
            if (dc == nullptr) return families;
            LOGFONTA every{};
            every.lfCharSet = DEFAULT_CHARSET;
            EnumFontFamiliesExA(dc, &every, AddFamily, reinterpret_cast<LPARAM>(&families), 0);
            DeleteDC(dc);
            return families;
        }
    }

    bool Menu::Draw(IGuiManager* gui, Settings& s, const MenuStatus& status)
    {
        if (!open || gui == nullptr) return false;
        // Ashita shares one style between every addon's windows: ours is set for this window and put back after it.
        ImGuiStyle& style       = gui->GetStyle();
        const ImGuiStyle ashita = style;
        ApplyPhoenixStyle(style);
        Ui ui{gui, style, style.Alpha};

        gui->SetNextWindowSize(ImVec2(kWindowWidth, kWindowHeight), ImGuiCond_Always);
        if (gui->Begin("HeadsUp", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar))
        {
            DrawTitle(ui, s, open, status.version);
            if (gui->BeginTable("##layout", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_BordersInnerV))
            {
                gui->TableSetupColumn("##pages", ImGuiTableColumnFlags_WidthFixed, kSidebarWidth, 0);
                gui->TableSetupColumn("##page", ImGuiTableColumnFlags_WidthFixed, kContentWidth, 0);
                gui->TableNextRow(0, 0.0f);
                gui->TableNextColumn();
                DrawSidebar(ui, m_Page);
                gui->TableNextColumn();
                const bool hasColors = m_Page == kOutlinesPage || m_Page == kNameplatesPage;
                if (!hasColors) m_ColorsTab = false;
                DrawTabs(ui, m_ColorsTab, hasColors);
                switch (m_Page)
                {
                case kOutlinesPage:
                    if (m_ColorsTab)
                        DrawOutlineColors(ui, s, m_Collapsed);
                    else
                        DrawOutlineSettings(ui, s, m_Collapsed, status);
                    break;
                case kNameplatesPage:
                    if (m_ColorsTab)
                        DrawNameplateColors(ui, s, m_Collapsed);
                    else
                    {
                        if (m_Fonts.empty()) m_Fonts = FontChoices(InstalledFontFamilies(), s.fontName);
                        DrawNameplateSettings(ui, s, m_Collapsed, m_Fonts);
                    }
                    break;
                default:
                    DrawDebug(ui, status, m_Collapsed, m_DebugRequested);
                    break;
                }
                gui->EndTable();
            }
        }
        gui->End();
        style = ashita;

        s = Clamp(s);
        return ui.changed;
    }

    bool Menu::TakeDebugRequest()
    {
        const bool requested = m_DebugRequested;
        m_DebugRequested     = false;
        return requested;
    }
}
