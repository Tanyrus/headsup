#include "menu.h"

#include "Ashita.h"

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
        // colors of phoenix-xi.com: square corners, 1 px borders and a red accent.
        constexpr ImVec4 kBackground   = Hex(0x180E0E, 0.96f);
        constexpr ImVec4 kCard         = Hex(0x291C1C);
        constexpr ImVec4 kControl      = Hex(0x321F1F);
        constexpr ImVec4 kHover        = Hex(0x3A2525);
        constexpr ImVec4 kBorder       = Hex(0xD2ABAB, 0.20f);
        constexpr ImVec4 kText         = Hex(0xFFF8F8);
        constexpr ImVec4 kHeading      = Hex(0xD2ABAB);
        constexpr ImVec4 kMuted        = Hex(0x8A6B6B);
        constexpr ImVec4 kAccent       = Hex(0xC55151);
        constexpr ImVec4 kAccentHover  = Hex(0xFF8D79);
        constexpr ImVec4 kDivider      = Hex(0xD2ABAB, 0.12f);
        constexpr ImVec4 kTrack        = Hex(0x180E0E);
        constexpr ImVec4 kGrabHeld     = Hex(0xD45E5E);
        constexpr ImVec4 kTint         = Hex(0xC55151, 0.30f); // a box being clicked
        constexpr ImVec4 kPicked       = Hex(0xC55151, 0.25f); // the selected page and tab
        constexpr ImVec4 kSelection    = Hex(0xC55151, 0.35f);
        constexpr ImVec4 kRowPicked    = Hex(0xC55151, 0.45f);
        constexpr ImVec4 kRowHovered   = Hex(0xC55151, 0.70f);
        constexpr ImVec4 kClear        = Hex(0x000000, 0.0f);

        // XIUI's config window layout: a sidebar of pages, each with settings and color settings tabs.
        constexpr float kSidebarWidth  = 170.0f;
        constexpr float kPageHeight    = 32.0f;  // a sidebar button
        constexpr float kAccentWidth   = 3.0f;   // the bar beside the selected page and under the selected tab
        constexpr float kContentWidth  = 440.0f;
        constexpr float kTabWidth      = 140.0f;
        constexpr float kTabHeight     = 28.0f;
        constexpr float kControlWidth  = 200.0f; // sliders
        constexpr float kNumberWidth   = 56.0f;  // the box to type a slider's value in
        constexpr float kTipWidth      = 300.0f;
        constexpr float kChipWidth     = 52.0f;
        constexpr float kOffAlpha      = 0.45f;  // settings of a feature that is off are drawn this faint
        // One size for every page, the tallest included; a longer page scrolls.
        constexpr float kWindowWidth   = 700.0f;
        constexpr float kWindowHeight  = 560.0f;
        // XIUI's corners.
        constexpr float kWindowRounding = 6.0f;
        constexpr float kRounding       = 4.0f;

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
            kOutlineSection    = 1u << 0,
            kMobsSection       = 1u << 1,
            kMobColorsSection  = 1u << 2,
            kDisplaySection    = 1u << 3,
            kTextSection       = 1u << 4,
            kTextColorsSection = 1u << 5,
            kConColorsSection  = 1u << 6,
            kDebugOutlines     = 1u << 7,
            kDebugNameplates   = 1u << 8,
            kDebugFrame        = 1u << 9,
        };

        const char* const kCategoryLabels[kCategoryCount] = {"Aggressive", "Passive", "No data", "Aggressive NM",
            "Passive NM"};
        const char* const kCategoryTips[kCategoryCount] = {
            "Aggressive mobs that would attack you at your level: not Too Weak, unless you are resting.",
            "Mobs that would leave you alone: passive, or aggressive but Too Weak for you.",
            "Mobs the Phoenix data does not list, so HeadsUp cannot tell.",
            "Notorious monsters that would attack you.",
            "Notorious monsters that would leave you alone."};
        // Ordinary mobs, notorious monsters, then mobs with no data.
        const int kCategoryOrder[kCategoryCount] = {0, 1, 3, 4, 2};
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
            s.WindowBorderSize = 1.0f;
            s.FrameBorderSize  = 0.0f;
            s.WindowPadding    = ImVec2(14.0f, 12.0f);
            s.FramePadding     = ImVec2(8.0f, 4.0f);
            s.ItemSpacing      = ImVec2(8.0f, 7.0f);
            s.CellPadding      = ImVec2(12.0f, 0.0f);
            s.GrabMinSize      = 10.0f;
        }

        struct ButtonColors
        {
            ImVec4 normal, hovered, active;
        };

        // Draws through Ashita's ImGui. PushStyleColor and PushStyleVar are overloaded, so colors and sizes are set in
        // the style itself while an item draws (tools/abi_check.py).
        struct Ui
        {
            IGuiManager* gui;
            ImGuiStyle& style;
            float alpha; // the window's own, before fading
            bool save = false;

            void Colored(const ImVec4& color, const char* text)
            {
                ImVec4& slot     = style.Colors[ImGuiCol_Text];
                const ImVec4 was = slot;
                slot             = color;
                gui->TextUnformatted(text);
                slot = was;
            }

            bool Button(const char* label, const ImVec2& size, const ButtonColors& colors)
            {
                ImVec4* c           = style.Colors;
                const ImVec4 was[3] = {c[ImGuiCol_Button], c[ImGuiCol_ButtonHovered], c[ImGuiCol_ButtonActive]};
                c[ImGuiCol_Button]        = colors.normal;
                c[ImGuiCol_ButtonHovered] = colors.hovered;
                c[ImGuiCol_ButtonActive]  = colors.active;
                const bool pressed        = gui->Button(label, size);
                c[ImGuiCol_Button]        = was[0];
                c[ImGuiCol_ButtonHovered] = was[1];
                c[ImGuiCol_ButtonActive]  = was[2];
                return pressed;
            }

            // A (?) after the last item; pointing at it explains the setting, even while the setting is faded.
            void Help(const char* tip)
            {
                gui->SameLine();
                Colored(kMuted, "(?)");
                Tip(tip);
            }

            // The tip, while the last item is pointed at.
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
            // Returns whether the section is open.
            bool Section(const char* title, uint32_t bit, uint32_t& collapsed)
            {
                gui->Spacing();
                const bool open     = (collapsed & bit) == 0;
                const ButtonColors header{kCard, kHover, kTint};
                // Headers share the window with the sidebar, whose pages can have the same names.
                char arrow[48], label[48];
                std::snprintf(arrow, sizeof(arrow), "##section arrow %s", title);
                std::snprintf(label, sizeof(label), "%s##section", title);
                ImVec4* c                 = style.Colors;
                const ImVec4 was[3]       = {c[ImGuiCol_Button], c[ImGuiCol_ButtonHovered], c[ImGuiCol_ButtonActive]};
                c[ImGuiCol_Button]        = header.normal;
                c[ImGuiCol_ButtonHovered] = header.hovered;
                c[ImGuiCol_ButtonActive]  = header.active;
                bool toggled              = gui->ArrowButton(arrow, open ? ImGuiDir_Down : ImGuiDir_Right);
                c[ImGuiCol_Button]        = was[0];
                c[ImGuiCol_ButtonHovered] = was[1];
                c[ImGuiCol_ButtonActive]  = was[2];
                gui->SameLine(0.0f, 0.0f);
                const ImVec2 align    = style.ButtonTextAlign;
                const ImVec4 text     = c[ImGuiCol_Text];
                style.ButtonTextAlign = ImVec2(0.0f, 0.5f);
                c[ImGuiCol_Text]      = kHeading;
                toggled |= Button(label, ImVec2(kContentWidth - gui->GetFrameHeight(), 0.0f), header);
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
                save |= gui->Checkbox(label, &value);
                Help(tip);
            }

            // A slider, then a box to type the value in, then the label. Typed values are clamped with the rest.
            void SliderInt(const char* label, int& value, int lo, int hi, const char* format, const char* tip)
            {
                char id[48];
                std::snprintf(id, sizeof(id), "##%s", label);
                gui->SetNextItemWidth(kControlWidth);
                gui->SliderInt(id, &value, lo, hi, format, ImGuiSliderFlags_AlwaysClamp);
                save |= gui->IsItemDeactivatedAfterEdit();
                gui->SameLine();
                gui->SetNextItemWidth(kNumberWidth);
                gui->InputInt(label, &value, 0, 0, 0);
                save |= gui->IsItemDeactivatedAfterEdit();
                Help(tip);
            }

            void SliderFloat(const char* label, float& value, float lo, float hi, const char* format, const char* number,
                const char* tip)
            {
                char id[48];
                std::snprintf(id, sizeof(id), "##%s", label);
                gui->SetNextItemWidth(kControlWidth);
                gui->SliderFloat(id, &value, lo, hi, format, ImGuiSliderFlags_AlwaysClamp);
                save |= gui->IsItemDeactivatedAfterEdit();
                gui->SameLine();
                gui->SetNextItemWidth(kNumberWidth);
                gui->InputFloat(label, &value, 0.0f, 0.0f, number, 0);
                save |= gui->IsItemDeactivatedAfterEdit();
                Help(tip);
            }

            void Swatch(const char* label, Color& color, const char* tip)
            {
                gui->ColorEdit3(label, color.v, ImGuiColorEditFlags_NoInputs);
                save |= gui->IsItemDeactivatedAfterEdit();
                Help(tip);
            }

            // An ON/OFF chip, filled when on and outlined when off, like the filter chips on phoenix-xi.com.
            void Chip(const char* id, bool& on)
            {
                ImVec4* c           = style.Colors;
                const ImVec4 was[2] = {c[ImGuiCol_Text], c[ImGuiCol_Border]};
                c[ImGuiCol_Text]      = on ? kText : kHeading;
                c[ImGuiCol_Border]    = on ? kAccent : kBorder;
                style.FrameBorderSize = 1.0f;
                char label[32];
                std::snprintf(label, sizeof(label), "%s##%s", on ? "ON" : "OFF", id);
                if (Button(label, ImVec2(kChipWidth, 0.0f), on ? ButtonColors{kAccent, kAccentHover, kAccent} : ButtonColors{kClear, kHover, kTint}))
                {
                    on   = !on;
                    save = true;
                }
                style.FrameBorderSize = 0.0f;
                c[ImGuiCol_Text]      = was[0];
                c[ImGuiCol_Border]    = was[1];
            }
        };

        // The window's own title row, in place of ImGui's title bar so the ON/OFF chip can sit beside the name. Dragging
        // any empty part of the window moves it.
        void DrawTitle(Ui& ui, Settings& s, bool& open)
        {
            ui.gui->AlignTextToFramePadding();
            ui.Colored(kText, "HeadsUp");
            ui.gui->SameLine();
            ui.Chip("enabled", s.enabled);
            ui.Tip("Turns every outline and nameplate on or off, the same as /hu on and /hu off.");
            ui.gui->SameLine();
            const float size = ui.gui->GetFrameHeight();
            ui.gui->SetCursorPosX(ui.gui->GetWindowWidth() - ui.style.WindowPadding.x - size);
            if (ui.Button("x##close", ImVec2(size, size), ButtonColors{kClear, kHover, kTint})) open = false;
            ui.Tip("Close. /hu opens it again.");
            ui.gui->Spacing();
        }

        // The pages, each a full-width button with an accent bar beside the selected one.
        void DrawSidebar(Ui& ui, int& page)
        {
            const ImVec2 spacing = ui.style.ItemSpacing;
            ui.style.ItemSpacing = ImVec2(0.0f, 2.0f);
            for (int p = 0; p < kPageCount; ++p)
            {
                const bool picked = p == page;
                char bar[16], label[32];
                std::snprintf(bar, sizeof(bar), "##bar%d", p);
                std::snprintf(label, sizeof(label), "%s##page", kPageLabels[p]);
                const ButtonColors accent = picked ? ButtonColors{kAccent, kAccent, kAccent} : ButtonColors{kClear, kClear, kClear};
                bool pressed              = ui.Button(bar, ImVec2(kAccentWidth, kPageHeight), accent);
                ui.gui->SameLine();
                const ButtonColors colors = picked ? ButtonColors{kPicked, kPicked, kPicked} : ButtonColors{kClear, kHover, kTint};
                pressed |= ui.Button(label, ImVec2(kSidebarWidth - kAccentWidth, kPageHeight), colors);
                if (pressed) page = p;
            }
            ui.style.ItemSpacing = spacing;
        }

        // "settings" and "color settings", each with an accent line under it while selected.
        void DrawTabs(Ui& ui, bool& colorsTab, bool hasColors)
        {
            const char* const labels[2] = {"settings", "color settings"};
            const int tabs              = hasColors ? 2 : 1;
            // No gap under the tabs: a row's gap is taken when its last item is placed.
            const float spacingY   = ui.style.ItemSpacing.y;
            ui.style.ItemSpacing.y = 0.0f;
            for (int t = 0; t < tabs; ++t)
            {
                const bool picked         = (t == 1) == colorsTab;
                const ButtonColors colors = picked ? ButtonColors{kPicked, kPicked, kPicked} : ButtonColors{kControl, kHover, kTint};
                if (t > 0) ui.gui->SameLine();
                if (ui.Button(labels[t], ImVec2(kTabWidth, kTabHeight), colors)) colorsTab = t == 1;
            }
            for (int t = 0; t < tabs; ++t)
            {
                const bool picked = (t == 1) == colorsTab;
                char line[16];
                std::snprintf(line, sizeof(line), "##line%d", t);
                if (t > 0) ui.gui->SameLine();
                const ButtonColors accent = picked ? ButtonColors{kAccent, kAccent, kAccent} : ButtonColors{kClear, kClear, kClear};
                ui.Button(line, ImVec2(kTabWidth, kAccentWidth), accent);
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
                for (const int c : kCategoryOrder)
                    ui.Check(kCategoryLabels[c], s.show[c], kCategoryTips[c]);
            }
            ui.Fade(false);
            if (!status.stencilAvailable) ui.Colored(kAccent, "Outlines are unavailable: the depth buffer has no stencil bits.");
        }

        void DrawOutlineColors(Ui& ui, Settings& s, uint32_t& collapsed)
        {
            ui.Fade(!s.enabled);
            if (ui.Section("Mob colors", kMobColorsSection, collapsed))
            {
                for (const int c : kCategoryOrder)
                {
                    ui.Fade(!s.enabled || !s.show[c]);
                    ui.Swatch(kCategoryLabels[c], s.color[c], kCategoryTips[c]);
                }
            }
            ui.Fade(false);
        }

        void DrawNameplateSettings(Ui& ui, Settings& s, uint32_t& collapsed)
        {
            ui.Fade(!s.enabled);
            if (ui.Section("Display", kDisplaySection, collapsed))
            {
                ui.Check("Replace game nameplates", s.replaceNameplates,
                    "Hides the game's mob names and draws them in the font below, in the game's color.");
                ui.Check("Show level and con", s.showLabels,
                    "Lv 20-23 EP-DC: the level range from the Phoenix data and how it cons to you. After you /check the "
                    "mob, its exact level, until it respawns.");
                ui.Check("Show icons", s.showIcons,
                    "XIUI's MobDB icons: aggressive or passive, whether it links, and how it detects you.");
                ui.Check("Hide behind walls", s.hideBehindWalls,
                    "Walls and terrain in front of a name cover its nameplate, like the game's own names, and the game's "
                    "menus sit on top. Off: nameplates are always on top.");
            }
            if (ui.Section("Text", kTextSection, collapsed))
            {
                // The slider's text is the font's name: ImGui's combo box and selectables are overloaded.
                ui.gui->SetNextItemWidth(kControlWidth);
                ui.gui->SliderInt("Font", &s.fontIndex, 0, kFontCount - 1, FontFamily(s.fontIndex), ImGuiSliderFlags_AlwaysClamp);
                ui.save |= ui.gui->IsItemDeactivatedAfterEdit();
                ui.gui->SameLine();
                ui.Check("Bold", s.fontBold, "Bold names and level text.");
                ui.Fade(!s.enabled || !s.replaceNameplates);
                ui.SliderInt("Name size", s.nameSize, kMinTextSize, kMaxTextSize, "%d px",
                    "The name's height, with Replace game nameplates on.");
                ui.Fade(!s.enabled || !s.showLabels);
                ui.SliderInt("Level and con size", s.labelSize, kMinTextSize, kMaxTextSize, "%d px", "The level line's height.");
                ui.Fade(!s.enabled || !s.showIcons);
                ui.SliderInt("Icon size", s.iconSize, kMinTextSize, kMaxTextSize, "%d px", "Each icon's width and height.");
                ui.Fade(!s.enabled);
                ui.Check("Scale with distance", s.scaleWithDistance,
                    "Sizes grow and shrink with the game's own name size as a mob comes closer or moves away.");
            }
            ui.Fade(false);
        }

        void DrawNameplateColors(Ui& ui, Settings& s, uint32_t& collapsed)
        {
            ui.Fade(!s.enabled);
            if (ui.Section("Name and text", kTextColorsSection, collapsed))
            {
                ui.Fade(!s.enabled || !s.replaceNameplates);
                ui.save |= ui.gui->Checkbox("##ownNameColor", &s.ownNameColor);
                ui.gui->SameLine();
                ui.Fade(!s.enabled || !s.replaceNameplates || !s.ownNameColor);
                ui.Swatch("Own name color", s.nameColor,
                    "Off: names keep the game's color, which shows who claimed the mob. On: every name takes this color.");
                ui.Fade(!s.enabled);
                ui.Swatch("Text outline", s.textOutline, "The edge around the name and the level text.");
                ui.Fade(!s.enabled || !s.showIcons);
                ui.Swatch("Icon tint", s.iconTint, "Multiplies the icons' colors. White keeps them as they are.");
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
                        ui.save |= ui.gui->IsItemDeactivatedAfterEdit();
                    }
                    ui.gui->EndTable();
                }
            }
            ui.Fade(false);
        }

        // Label and value rows; the table lines the values up.
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
            char outlined[32], meshes[32], shown[32], letters[64], frame[48], level[32];
            if (ui.Section("Outlines", kDebugOutlines, collapsed))
            {
                std::snprintf(outlined, sizeof(outlined), "%u", status.outlinedMobs);
                std::snprintf(meshes, sizeof(meshes), "%u", status.meshes);
                DebugRows(ui, "##debugOutlines", {{"Mobs outlined", outlined}, {"Meshes outlined last frame", meshes},
                    {"Stencil buffer", status.stencilAvailable ? "available" : "missing: no outlines"}});
            }
            if (ui.Section("Nameplates", kDebugNameplates, collapsed))
            {
                std::snprintf(shown, sizeof(shown), "%u", status.nameplates);
                std::snprintf(letters, sizeof(letters), "%u in the scene, %u from mobs, %u hidden", status.lettersInScene,
                    status.lettersFromMobs, status.lettersHidden);
                DebugRows(ui, "##debugNameplates", {{"Nameplates shown", shown},
                    {"Drawn", status.drewInScene ? "into the scene, behind walls" : "on top"}, {"Game name letters", letters}});
            }
            if (ui.Section("Frame", kDebugFrame, collapsed))
            {
                std::snprintf(frame, sizeof(frame), "%.2f ms (%.0f fps)", status.frameMs,
                    status.frameMs > 0.0 ? 1000.0 / status.frameMs : 0.0);
                if (status.playerLevel > 0)
                    std::snprintf(level, sizeof(level), "%d%s", status.playerLevel, status.sitting ? ", resting" : "");
                else
                    std::snprintf(level, sizeof(level), "unknown");
                DebugRows(ui, "##debugFrame", {{"Frame time", frame}, {"Your level", level}});
            }
            ui.gui->Spacing();
            if (ui.Button("Write debug report", ImVec2(0.0f, 0.0f), ButtonColors{kControl, kHover, kTint})) requested = true;
            ui.Help("The same as /hu debug: writes what every mob's outline and nameplate show to logs/headsup, then "
                    "records 120 frames of nameplate data.");
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
            DrawTitle(ui, s, open);
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
                        DrawNameplateSettings(ui, s, m_Collapsed);
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
        return ui.save;
    }

    bool Menu::TakeDebugRequest()
    {
        const bool requested = m_DebugRequested;
        m_DebugRequested     = false;
        return requested;
    }
}
