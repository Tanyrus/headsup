#include "menu_pages.h"

#include "bar_theme.h"
#include "menu_state.h"
#include "menu_ui.h"
#include "native_locate.h"

#include <cctype>
#include <cstdio>
#include <algorithm>
#include <iterator>
#include <optional>
#include <string_view>
#include <utility>

namespace headsup
{
    namespace
    {
        const char* const kBarWidgetLabels[] = {"Zone", "Weather", "Vana'diel", "Target", "Gil",
            "Bags", "Clock", "Game help", "Server time", "FPS", "Job", "Connection", "Party", "Conquest",
            "Signet", "Distance"};
        const char* const kBarWidgetTips[] = {
            "The zone, its region and who holds it. Hover for the influence there and the conquest tally.",
            "The weather now and today's chance of it. Hover for the forecast.",
            "The Vana'diel day, time and moon.",
            "Your target's name, level and con. Hover a mob for its details.",
            "Gil, or another currency. Hover for every currency.",
            "How full your inventory is. Hover for every bag.",
            "Your computer's time.",
            "The game's help text, in a bar of its own under the top bar.",
            "The server's time, or another zone's. Hover for the daily reset and conquest tally.",
            "Frames per second. Hover for frame times.",
            "Jobs, levels and EXP to the next level, with a bar. Hover for EXP per hour.",
            "Bytes sent and received each second; amber or red when data stops. Hover for latency.",
            "Party size and the lowest HP and MP. Hover for each member.",
            "Time to the conquest tally and your conquest points.",
            "Time left on Signet or Sanction, while one is on.",
            "How far your target is, in yalms."};
        static_assert(std::size(kBarWidgetLabels) == kBarWidgetCount && std::size(kBarWidgetTips) == kBarWidgetCount);
        const char* const kBarSectionLabels[] = {"Left", "Center", "Right"};
        static_assert(std::size(kBarSectionLabels) == kBarSectionCount);
        constexpr const char* kThemeFont = "Theme's"; // a bar font left to its theme

        // Which widgets have a second line, or icons, for their options to offer switching off.
        bool HasSecondLine(BarWidget w)
        {
            return w != BarWidget::EarthTime && w != BarWidget::Fps && w != BarWidget::Connection &&
                   w != BarWidget::Distance && w != BarWidget::GameHelp;
        }
        bool HasIcons(BarWidget w) { return w == BarWidget::Weather || w == BarWidget::Target || w == BarWidget::Gil; }

        // The game's windows HeadsUp moves out from under the help bar, and those it has seen there.
        // Whether text holds part, in any case.
        bool Matches(std::string_view text, const char* part)
        {
            const std::string_view p(part);
            if (p.empty()) return true;
            const auto lower = [](char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); };
            for (size_t at = 0; at + p.size() <= text.size(); ++at)
                if (std::equal(p.begin(), p.end(), text.begin() + static_cast<std::ptrdiff_t>(at),
                        [&](char a, char b) { return lower(a) == lower(b); }))
                    return true;
            return false;
        }

        void DrawMovedWindows(Ui& ui, Settings& s, const MenuStatus& status, WindowList& list)
        {
            ui.Switch("Every Window", s.allUnderBar, "Every window that opens under the bar goes below it, not only those "
                                                     "switched on or grouped.");
            ui.Switch("Game Help", s.moveGameHelp,
                "The game's own help bar goes under the top bar, and the windows under it. Off while it is hidden.");
            {
                const float y = ui.gui->GetCursorPosY();
                if (ui.Action("HeadsUp Defaults", "Every window under the bar, the game's help too, in the groups the game "
                                                  "opens them in. Offsets are cleared."))
                {
                    UseHeadsUpWindowDefaults(s);
                    ui.changed = true;
                }
                ui.gui->SameLine();
                if (ui.WithText(kLabel, [&] {
                        return ui.Shape("HeadsUp Help", ImVec2(0.0f, 0.0f), kRaised, ui.gui->GetFrameHeight() / 2.0f,
                            kRaisedEdge);
                    }))
                {
                    UseHelpTextDefaults(s);
                    ui.changed = true;
                }
                ui.Tip("HeadsUp's help text in place of the game's help bar, which is hidden, and every window under it.");
                ui.gui->SameLine();
                if (ui.WithText(kLabel, [&] {
                        return ui.Shape("Game Defaults", ImVec2(0.0f, 0.0f), kFlat, ui.gui->GetFrameHeight() / 2.0f,
                            kRaisedEdge);
                    }))
                {
                    UseGameWindowDefaults(s);
                    ui.changed = true;
                }
                ui.Tip("Every window where the game puts it: nothing moved, no groups, no offsets.");
                ui.EndRowAt(ui.cardX, y, ui.gui->GetFrameHeight() + ui.style.ItemSpacing.y);
            }
            ui.Note("Under Bar takes a window below the help bar, all together; X and Y move it from where the game "
                    "puts it. Drag a number, or Ctrl+click it to type.");
            {
                const float y = ui.Row("Find", nullptr, ui.Em(kFilterWidth) + ui.style.ItemSpacing.x + ui.Em(kSwitchWidth));
                const float x = ui.gui->GetCursorPosX();
                ui.gui->SetNextItemWidth(ui.Em(kFilterWidth));
                ui.SquareFrames([&] {
                    return ui.gui->InputTextWithHint("##find window", "Name", list.filter, sizeof(list.filter), 0,
                        nullptr, nullptr);
                });
                ui.gui->SetCursorPos(ImVec2(x + ui.Em(kFilterWidth) + ui.style.ItemSpacing.x, y));
                const bool was = ui.changed;
                ui.SwitchAt("##all windows", list.all);
                ui.changed = was;
                ui.Tip("Every window the game has, not only the named ones and yours.");
                ui.EndRowAt(ui.cardX, y, ui.gui->GetFrameHeight());
            }
            // The named windows and the player's, or every one, less those the filter leaves out.
            std::vector<std::string> listed;
            const auto add = [&](const std::string& name) {
                const char* label = WindowLabel(name);
                if (std::ranges::find(listed, name) == listed.end() &&
                    (Matches(name, list.filter) || (label != nullptr && Matches(label, list.filter))))
                    listed.push_back(name);
            };
            // In one order whatever is switched, so a row stays put when it changes: the named windows, then the
            // rest; one the player set up or that opened shows without Show All.
            const auto has = [](const auto& names, const std::string& name) {
                return std::ranges::find(names, name) != names.end();
            };
            for (const std::string& name : AllWindows())
                if (list.all || WindowLabel(name) != nullptr || has(s.movedWindows, name) ||
                    std::ranges::any_of(s.windowOffsets, [&](const WindowOffset& o) { return o.name == name; }) ||
                    (status.seenWindows != nullptr && has(*status.seenWindows, name)))
                    add(name);
            for (const std::string& window : listed)
            {
                const auto at = std::ranges::find(s.movedWindows, window);
                bool moved    = at != s.movedWindows.end();
                const char* label = WindowLabel(window);
                char text[64];
                std::snprintf(text, sizeof(text), "%s##window %s", label != nullptr ? label : window.c_str(),
                    window.c_str());
                // Its row: the switch to go under the bar, then its offset.
                const float frame = ui.gui->GetFrameHeight();
                const float width = ui.Em(kSwitchWidth) + ui.style.ItemSpacing.x * 2.0f +
                                    2.0f * (ui.Em(kNumberName) + ui.Em(kNumberWidth));
                const float y = ui.Row(text, "Under Bar, then its X and Y offset.", width);
                const float x = ui.gui->GetCursorPosX();
                const bool was = moved;
                char id[64];
                std::snprintf(id, sizeof(id), "##under %s", window.c_str());
                ui.SwitchAt(id, moved);
                if (moved != was)
                {
                    if (moved)
                        s.movedWindows.push_back(window);
                    else
                        s.movedWindows.erase(at);
                }
                auto offset = std::ranges::find(s.windowOffsets, window, &WindowOffset::name);
                int dx = offset != s.windowOffsets.end() ? offset->x : 0, dy = offset != s.windowOffsets.end() ? offset->y : 0;
                const int oldX = dx, oldY = dy;
                const float numbers = x + ui.Em(kSwitchWidth) + ui.style.ItemSpacing.x * 2.0f;
                ui.gui->SetCursorPos(ImVec2(numbers, y));
                std::snprintf(id, sizeof(id), "##x %s", window.c_str());
                ui.NumberAt("X", id, dx, kMaxWindowOffset);
                ui.gui->SetCursorPos(ImVec2(numbers + ui.Em(kNumberName) + ui.Em(kNumberWidth) + ui.style.ItemSpacing.x, y));
                std::snprintf(id, sizeof(id), "##y %s", window.c_str());
                ui.NumberAt("Y", id, dy, kMaxWindowOffset);
                ui.EndRowAt(ui.cardX, y, frame);
                if (dx == oldX && dy == oldY) continue;
                if (offset == s.windowOffsets.end())
                    s.windowOffsets.push_back(WindowOffset{window, dx, dy});
                else
                {
                    offset->x = dx;
                    offset->y = dy;
                }
            }
        }

        // A window as the menu names it: HeadsUp's name for it, or the game's.
        std::string WindowName(const std::string& window)
        {
            const char* label = WindowLabel(window);
            return label != nullptr ? label : window;
        }

        // Each group: its name to type, its Under Bar switch and offset, a remove button, then its windows as chips a
        // click takes out and a dropdown that adds one no group has.
        void DrawWindowGroups(Ui& ui, Settings& s)
        {
            ui.Note("Windows in a group move by its offset together, keeping their places beside each other. Each "
                    "window's own X and Y below still moves it within the group.");
            std::optional<size_t> removed;
            for (size_t g = 0; g < s.windowGroups.size(); ++g)
            {
                WindowGroup& group = s.windowGroups[g];
                const float frame  = ui.gui->GetFrameHeight();
                const float y      = ui.gui->GetCursorPosY();
                const float left   = ui.cardX + ui.Em(kCardPad);
                // Its name.
                ui.gui->SetCursorPos(ImVec2(left, y));
                char name[kMaxGroupName + 1] = {};
                std::snprintf(name, sizeof(name), "%s", group.name.c_str());
                char id[48];
                std::snprintf(id, sizeof(id), "##group name %zu", g);
                ui.gui->SetNextItemWidth(ui.Em(kGroupNameWidth));
                ui.SquareFrames([&] { return ui.gui->InputText(id, name, sizeof(name), 0, nullptr, nullptr); });
                if (ui.gui->IsItemDeactivatedAfterEdit() && name[0] != '\0')
                {
                    group.name = name;
                    ui.changed = true;
                }
                // Under Bar, its offset and remove, from the card's right edge.
                const float numbers = 2.0f * (ui.Em(kNumberName) + ui.Em(kNumberWidth)) + ui.style.ItemSpacing.x;
                const float right   = ui.cardX + ui.cardWidth - ui.Em(kCardPad);
                float x             = right - frame - ui.style.ItemSpacing.x - numbers - ui.style.ItemSpacing.x * 2.0f -
                          ui.Em(kSwitchWidth);
                ui.gui->SetCursorPos(ImVec2(x, y));
                std::snprintf(id, sizeof(id), "##group under %zu", g);
                ui.SwitchAt(id, group.underBar);
                ui.gui->SetCursorPos(ImVec2(x, y));
                ui.Tip("Under Bar: the whole group goes below the help bar.");
                x += ui.Em(kSwitchWidth) + ui.style.ItemSpacing.x * 2.0f;
                ui.gui->SetCursorPos(ImVec2(x, y));
                std::snprintf(id, sizeof(id), "##group x %zu", g);
                ui.NumberAt("X", id, group.x, kMaxWindowOffset);
                ui.gui->SetCursorPos(ImVec2(x + ui.Em(kNumberName) + ui.Em(kNumberWidth) + ui.style.ItemSpacing.x, y));
                std::snprintf(id, sizeof(id), "##group y %zu", g);
                ui.NumberAt("Y", id, group.y, kMaxWindowOffset);
                ui.gui->SetCursorPos(ImVec2(right - frame, y));
                std::snprintf(id, sizeof(id), "x##group remove %zu", g);
                if (ui.WithText(kMuted, [&] { return ui.Shape(id, ImVec2(frame, frame), kRaised, frame / 2.0f); }))
                    removed = g;
                ui.Tip("Removes the group; its windows stay where the game puts them.");
                ui.EndRowAt(ui.cardX, y, frame);
                // Its windows, then the one to add.
                const float rowLeft = left + ui.Em(1.0f);
                std::optional<size_t> taken;
                for (size_t w = 0; w < group.windows.size(); ++w)
                {
                    const std::string shown = WindowName(group.windows[w]) + "  x";
                    char chip[64];
                    std::snprintf(chip, sizeof(chip), "%s##member %zu %zu", shown.c_str(), g, w);
                    ui.PlaceChip(w == 0, rowLeft, ui.ChipRoom(shown.size()));
                    if (ui.Shape(chip, ImVec2(0.0f, 0.0f), kRaised, frame / 2.0f, kRaisedEdge)) taken = w;
                }
                if (taken)
                {
                    group.windows.erase(group.windows.begin() + static_cast<std::ptrdiff_t>(*taken));
                    ui.changed = true;
                }
                std::vector<std::string> free;
                for (const std::string& window : AllWindows())
                    if (std::ranges::none_of(s.windowGroups, [&](const WindowGroup& other) {
                            return std::ranges::find(other.windows, window) != other.windows.end();
                        }))
                        free.push_back(window);
                std::snprintf(id, sizeof(id), "##group add %zu", g);
                int chosen = 0;
                ui.PlaceChip(group.windows.empty(), rowLeft, ui.Em(kControlWidth) * 0.8f);
                if (ui.ListPopup(id, "+ Add Window", ui.Em(kControlWidth) * 0.8f, static_cast<int>(free.size()),
                        [&](int i) { return WindowName(free[static_cast<size_t>(i)]); }, -1, chosen))
                {
                    group.windows.push_back(free[static_cast<size_t>(chosen)]);
                    ui.changed = true;
                }
                ui.gui->Spacing();
            }
            if (removed)
            {
                s.windowGroups.erase(s.windowGroups.begin() + static_cast<std::ptrdiff_t>(*removed));
                ui.changed = true;
            }
            if (s.windowGroups.size() < kMaxWindowGroups && ui.Action("+ New Group"))
            {
                s.windowGroups.push_back(
                    WindowGroup{"Group " + std::to_string(s.windowGroups.size() + 1), 0, 0, false, {}});
                ui.changed = true;
            }
        }

        // The section a widget is in, and where.
        std::pair<int, size_t> PlaceOf(const BarPlacement& placement, BarWidget widget)
        {
            for (int section = 0; section < kBarSectionCount; ++section)
            {
                const std::vector<BarWidget>& list = placement[section];
                if (const auto at = std::ranges::find(list, widget); at != list.end())
                    return {section, static_cast<size_t>(at - list.begin())};
            }
            return {0, 0};
        }

        // Left, Center and Right as trays sunk into the card, each holding its widgets as raised chips in bar order:
        // a chip drags to another place or tray, and a click picks it for its options below. A lit dot says it is on.
        void DrawTrays(Ui& ui, Settings& s, int& picked, std::optional<Drop>& drop)
        {
            const float gap   = ui.Em(0.6f);
            const float trayW = (ui.cardWidth - 2.0f * ui.Em(kCardPad) - 2.0f * gap) / kBarSectionCount;
            const float chipH = ui.Em(kChipHeight), inset = ui.Em(0.45f), head = ui.Em(1.7f), step = chipH + ui.Em(0.3f);
            size_t most       = 1;
            for (const std::vector<BarWidget>& list : s.barSections)
                most = std::max(most, list.size() + 1); // a free row to drop at the end
            const float trayH = head + static_cast<float>(most) * step + inset;
            const float top   = ui.gui->GetCursorPosY();
            for (int section = 0; section < kBarSectionCount; ++section)
            {
                const auto here = static_cast<BarSection>(section);
                const float x   = ui.cardX + ui.Em(kCardPad) + static_cast<float>(section) * (trayW + gap);
                ui.gui->SetCursorPos(ImVec2(x, top));
                char id[64];
                std::snprintf(id, sizeof(id), "##tray %d", section);
                ui.Ground(id, ImVec2(trayW, trayH), kTray, ui.Em(kCardRadius), kBorder);
                ui.gui->SetCursorPos(ImVec2(x + inset + ui.Em(0.3f), top + ui.Em(0.35f)));
                ui.Text(kMuted, kBarSectionLabels[section]);
                const std::vector<BarWidget>& widgets = s.barSections[section];
                float y = top + head;
                for (size_t i = 0; i < widgets.size(); ++i)
                {
                    const int w   = static_cast<int>(widgets[i]);
                    const bool on = s.barWidgets[w];
                    ui.gui->SetCursorPos(ImVec2(x + inset, y));
                    std::snprintf(id, sizeof(id), "##chip %d", w);
                    ui.gui->SetNextItemAllowOverlap();
                    if (ui.Shape(id, ImVec2(trayW - 2.0f * inset, chipH), kRaised, chipH / 2.0f,
                            w == picked ? kAccent : kRaisedEdge))
                        picked = w;
                    if (!ui.gui->IsItemActive()) ui.Tip(kBarWidgetTips[w]);
                    if (ui.gui->BeginDragDropSource(0))
                    {
                        const DraggedWidget dragged{static_cast<uint8_t>(section), static_cast<uint8_t>(i)};
                        ui.gui->SetDragDropPayload(kWidgetPayload, &dragged, sizeof(dragged), 0);
                        ui.Title(kHeading, TitleCase(kBarWidgetLabels[w]).c_str(), kCardTitleScale);
                        ui.gui->EndDragDropSource();
                    }
                    ui.DropTarget(here, i, drop); // dropped on a chip: before it
                    // Its dot and name over it, drawn disabled so the chip keeps every click.
                    const float dot = ui.Em(kChipDot);
                    ui.gui->BeginDisabled(true);
                    ui.gui->SetCursorPos(ImVec2(x + inset + ui.Em(0.7f), y + (chipH - dot) / 2.0f));
                    std::snprintf(id, sizeof(id), "##dot %d", w);
                    ui.Shape(id, ImVec2(dot, dot), on ? kAccentButton : ButtonColors{kMuted, kMuted, kMuted}, dot / 2.0f);
                    ui.gui->SetCursorPos(ImVec2(x + inset + ui.Em(1.6f), y + (chipH - ui.gui->GetFontSize()) / 2.0f));
                    ui.Text(on ? kText : kMuted, TitleCase(kBarWidgetLabels[w]).c_str());
                    ui.gui->EndDisabled();
                    y += step;
                }
                // The rest of the tray takes a drop at the section's end.
                ui.gui->SetCursorPos(ImVec2(x, y));
                std::snprintf(id, sizeof(id), "##end %d", section);
                ui.gui->InvisibleButton(id, ImVec2(trayW, std::max(chipH, top + trayH - y)));
                ui.DropTarget(here, widgets.size(), drop);
            }
            ui.EndRowAt(ui.cardX, top, trayH + ui.Em(0.3f));
        }

        // One widget's options: its section, what it shows and its own settings.
        void DrawWidgetOptions(Ui& ui, Settings& s, BarWidget widget)
        {
            const int w = static_cast<int>(widget);
            ui.Note(kBarWidgetTips[w]);
            ui.Fade(!BarOn(s) || !s.barWidgets[w]);
            auto [section, index] = PlaceOf(s.barSections, widget);
            int moved             = section;
            ui.Segments("Section", moved, kBarSectionLabels, kBarSectionCount, nullptr);
            if (moved != section)
                ui.changed |= MoveWidget(s.barSections, static_cast<BarSection>(section), index,
                    static_cast<BarSection>(moved), s.barSections[moved].size());
            if (HasSecondLine(widget)) ui.Switch("Second line", s.barSecondLine[w], nullptr);
            if (HasIcons(widget)) ui.Switch("Icons", s.barIcons[w], nullptr);
            switch (widget)
            {
            case BarWidget::Target:
                ui.Switch("Details", s.barTargetDetails, "Aggro icons after the name, weaknesses after the level.");
                break;
            case BarWidget::Gil:
                ui.NumberDropdown("Currency", s.barCurrency, kGilCurrency, kCurrencyCount - 1,
                    [](int c) { return std::string(kCurrencyTable[c].name); }, nullptr);
                break;
            case BarWidget::EarthTime:
                ui.Switch("24-hour", s.clock24, "Also Server time.");
                ui.Switch("Seconds", s.clockSeconds, nullptr);
                break;
            case BarWidget::ServerTime:
            {
                int zone = static_cast<int>(s.serverClockZone);
                ui.NumberDropdown("Time zone", zone, 0, kClockZoneCount - 1,
                    [](int z) { return std::string(ClockZoneName(static_cast<ClockZone>(z))); },
                    "Resets still go by Japan's.");
                s.serverClockZone = static_cast<ClockZone>(zone);
                ui.Switch("24-hour", s.clock24, "Also the Clock.");
                ui.Switch("Seconds", s.serverSeconds, nullptr);
                break;
            }
            case BarWidget::Job:
            {
                const char* const places[kExpBarPlaceCount] = {"Under", "Beside", "Off"};
                ui.Segments("EXP bar", s.expBar, places, kExpBarPlaceCount, nullptr);
                ui.Switch("Job names", s.jobNames, "Off, the levels alone.");
                break;
            }
            case BarWidget::Connection: ui.Switch("Latency", s.barLatency, "After the rates, once measured."); break;
            default: break;
            }
        }
    }

    void DrawUserInterfacePage(Ui& ui, Settings& s, const MenuStatus& status,
        std::unordered_set<std::string>& collapsed, WindowList& windows)
    {
        ui.Fade(false);
        if (ui.BeginCard("Window Groups", collapsed))
        {
            DrawWindowGroups(ui, s);
            ui.EndCard();
        }
        if (ui.BeginCard("Game Windows", collapsed))
        {
            DrawMovedWindows(ui, s, status, windows);
            ui.EndCard();
        }
        ui.Fade(false);
    }

    void DrawTopBarPage(Ui& ui, Settings& s, const MenuStatus& status, std::unordered_set<std::string>& collapsed,
        const std::vector<std::string>& fonts, int& picked)
    {
        ui.Fade(!s.enabled);
        if (ui.BeginCard("Top Bar", collapsed, &s.showBar,
                "Hover a widget for details; click it to keep them open."))
        {
            ui.Fade(!BarOn(s));
            int theme = static_cast<int>(s.barTheme);
            ui.NumberDropdown("Theme", theme, 0, kBarThemeCount - 1,
                [](int t) { return std::string(BarThemeName(static_cast<BarTheme>(t))); }, nullptr);
            s.barTheme = static_cast<BarTheme>(theme);
            const char* const separators[kBarSeparatorsCount] = {"Theme's", "Diamonds", "None"};
            ui.Segments("Separators", s.barSeparators, separators, kBarSeparatorsCount, nullptr,
                ui.Em(kWideSegmentWidth));
            ui.FontDropdown("Title font", s.barTitleFont, fonts, nullptr, kThemeFont);
            ui.FontDropdown("Text font", s.barTextFont, fonts, nullptr, kThemeFont);
            ui.SliderInt("Height", s.barHeight, kMinBarHeight, kMaxBarHeight, "%d px", nullptr);
            char textSize[32];
            if (s.barTextSize == kAutoBarTextSize)
                std::snprintf(textSize, sizeof(textSize), "Auto");
            else if (status.barTitlePixels > 0 && status.barTitlePixels != s.barTextSize)
                std::snprintf(textSize, sizeof(textSize), "%%d px (%d fits)", status.barTitlePixels);
            else
                std::snprintf(textSize, sizeof(textSize), "%%d px");
            ui.SliderInt("Text size", s.barTextSize, kAutoBarTextSize, kMaxBarTextSize, textSize,
                "Auto follows the height. Too large for two lines, the second line goes as One Line says.");
            const char* const oneLine[kSubtitlePlaceCount] = {"Hide", "Before", "After"};
            ui.Segments("One line", s.barSubtitle, oneLine, kSubtitlePlaceCount,
                "On a bar too short for two lines, each widget's second line goes before or after its title, or is "
                "left off.");
            ui.SliderInt("Popup size", s.popupTextSize, kAutoPopupTextSize, kMaxBarTextSize,
                s.popupTextSize == kAutoPopupTextSize ? "Auto" : "%d px", "Auto follows the height.");
            ui.Fade(!s.enabled);
            ui.Switch("Hide readout", s.hideNetstat, "The game's S, R and % numbers at the top right.");
            ui.EndCard();
        }
        const int help = static_cast<int>(BarWidget::GameHelp);
        ui.Fade(!BarOn(s));
        if (ui.BeginCard("Help Bar", collapsed, &s.barWidgets[help], kBarWidgetTips[help]))
        {
            ui.Fade(!s.enabled);
            ui.Switch("Hide game help", s.hideHelpBar, "The game's own help bar, which the top bar covers.");
            ui.Fade(!BarOn(s) || !s.barWidgets[help]);
            ui.Offset("Position", s.helpBarX, s.helpBarY, kMaxHelpBarOffset,
                "Pixels from its place under the top bar.");
            ui.Switch("Background", s.helpBarBackground, "Off, the help text alone, with no bar behind it.");
            ui.EndCard();
        }
        ui.Fade(!BarOn(s));
        if (ui.BeginCard("Widgets", collapsed, nullptr, "Drag a chip to move it; click it for its options."))
        {
            std::optional<Drop> drop; // applied after drawing, so no list changes under its own loop
            DrawTrays(ui, s, picked, drop);
            // Read after the trays, where a click picks: the options are the clicked chip's in the same frame.
            if (picked == static_cast<int>(BarWidget::GameHelp) || picked < 0 || picked >= kBarWidgetCount) picked = 0;
            const int w = picked;
            if (drop) ui.changed |= MoveWidget(s.barSections, drop->from, drop->index, drop->to, drop->at);
            if (ui.Action("Reset Layout", "Puts every widget back in its first place. Switches stay.") &&
                s.barSections != DefaultPlacement())
            {
                s.barSections = DefaultPlacement();
                ui.changed    = true;
            }
            ui.Note(("Options for " + TitleCase(kBarWidgetLabels[w])).c_str());
            ui.Switch("Enabled", s.barWidgets[w], nullptr);
            DrawWidgetOptions(ui, s, static_cast<BarWidget>(w));
            ui.EndCard();
        }
        ui.Fade(false);
    }
}
