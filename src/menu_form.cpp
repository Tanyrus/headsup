#include "menu_form.h"

#include "bar_theme.h"
#include "icons.h"
#include "menu_text.h"
#include "native_locate.h"
#include "plate_settings.h"
#include "plate_template.h"
#include "pointer_keys.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <optional>
#include <utility>

namespace headsup
{
    namespace
    {
        constexpr const char* kChip        = "btn raised chip";
        constexpr const char* kQuietChip   = "btn flat chip";
        constexpr const char* kNavStart    = "nav-";
        constexpr const char* kWidgetChip  = "chip "; // a tray chip's name in its card, before its widget's number

        std::string Fmt(const char* format, auto... values)
        {
            char text[96];
            std::snprintf(text, sizeof(text), format, values...);
            return text;
        }

        // The pages as pills, the picked one filled; a child page is indented.
        void Sidebar(Form& form, MenuFormState& state)
        {
            form.BeginBlock(kSidebarBlock);
            for (const PageInfo& info : kPages)
            {
                const std::string id = kNavStart + PageKey(info.page);
                if (form.Clicked(id) && state.page != info.page)
                {
                    state.page       = info.page;
                    form.scrollToTop = true;
                }
                form.Raw(std::string("<div class=\"nav") + (info.child ? " child" : "") + "\" id=\"" + id + "\">" +
                         EscapedRml(info.label) + "</div>");
            }
            for (const PageInfo& info : kPages)
                form.Set(kNavStart + PageKey(info.page), FormValueKind::Class, "picked", {}, info.page == state.page);
            form.EndBlock();
        }

        // A row of keys: each a chip that a click removes, and while there are fewer than most, a chip that takes
        // the next key pressed that was not already held.
        void Keys(Form& form, const char* label, const char* tip, std::vector<uint8_t>& keys, size_t most,
            KeyCapture& capture)
        {
            const std::string name = label;
            form.Row(label, tip);
            form.EndRow();
            form.Line("line chips");
            std::optional<size_t> removed;
            for (size_t i = 0; i < keys.size(); ++i)
                if (form.Button((KeyName(keys[i]) + "  x").c_str(), (name + " key " + std::to_string(i)).c_str(), kChip))
                    removed = i;
            if (capture.on)
            {
                if (form.Button("Press a Key...", (name + " cancel").c_str(), "btn flat chip waiting"))
                    capture.on = false;
                else if (form.keyDown)
                    for (const uint8_t key : HeldKeys(form.keyDown))
                    {
                        if (std::ranges::find(capture.held, key) != capture.held.end()) continue;
                        if (std::ranges::find(keys, key) == keys.end()) keys.push_back(key);
                        form.changed = true;
                        capture.on   = false;
                        break;
                    }
            }
            else if (keys.size() < most && form.Button("+ Add Key", (name + " add").c_str(), kQuietChip))
                capture = KeyCapture{true, form.keyDown ? HeldKeys(form.keyDown) : std::vector<uint8_t>{}};
            form.EndLine();
            if (removed)
            {
                keys.erase(keys.begin() + static_cast<std::ptrdiff_t>(*removed));
                form.changed = true;
            }
        }

        // Every setting back to HeadsUp's defaults, after a second press.
        void Reset(Form& form, Settings& s, bool& confirm)
        {
            if (!form.BeginCard("Reset")) return;
            if (!confirm)
            {
                if (form.Action("Reset All Settings", "Every setting on every page back to its default.")) confirm = true;
            }
            else
            {
                form.Note("Every setting goes back to its default. This cannot be undone.");
                form.Line();
                if (form.Button("Reset Everything", "reset everything", kAccentPill))
                {
                    s            = Settings{};
                    form.changed = true;
                    confirm      = false;
                }
                if (form.Button("Keep Them", "keep them", kRaisedPill)) confirm = false;
                form.EndLine();
            }
            form.EndCard();
        }

        void ConfigMenuPage(Form& form, Settings& s, MenuFormState& state, const std::vector<std::string>& menuFonts)
        {
            if (form.BeginCard("Settings Window"))
            {
                int theme = static_cast<int>(s.menuTheme);
                form.NumberDropdown("Theme", theme, 0, kMenuThemeCount - 1,
                    [](int t) { return std::string(MenuThemeName(static_cast<MenuTheme>(t))); },
                    "The window's colors and light.", 0.0f, [](int t) { return std::string(MenuThemeTip(static_cast<MenuTheme>(t))); });
                s.menuTheme = static_cast<MenuTheme>(theme);
                form.FontDropdown("Font", s.menuFont, menuFonts, "Titles stay in the bar's font.");
                form.SliderInt("Size", s.menuScale, kMinMenuScale, kMaxMenuScale, "%d%%",
                    "Drag its corner to resize the window.");
                form.EndCard();
            }
            Reset(form, s, state.confirmReset);
        }

        void MouseCursorPage(Form& form, Settings& s, MenuFormState& state)
        {
            form.Fade(!s.enabled);
            if (form.BeginCard("Mouse Pointer"))
            {
                form.Switch("Chocobo", s.chocoboPointer, "PlayOnline's chocobo as the mouse pointer.");
                form.Switch("Pointer fix", s.fixPointer,
                    "Keeps the game's pointer under yours after a resize, and clicks where you click. It also shows the "
                    "pointer again over Ashita's windows.");
                Keys(form, "Keep pointer",
                    "The game hides its pointer when a key runs a command. While only these keys are held, it stays.",
                    s.keepPointerKeys, kMaxKeptKeys, state.keyCapture);
                form.EndCard();
            }
            form.Fade(false);
        }

        void DebugPage(Form& form, Settings& s, const MenuReadings& r, MenuFormState& state)
        {
            if (form.BeginCard("HeadsUp", &s.enabled, "All of HeadsUp, as /hu on and /hu off."))
            {
                if (form.Action("Debug Report", "As /hu debug: writes a report to logs/headsup.")) state.debugRequested = true;
                form.EndCard();
            }
            if (form.BeginCard("This Frame"))
            {
                form.Reading("Mobs outlined", std::to_string(r.outlinedMobs));
                form.Reading("Meshes outlined", std::to_string(r.meshes));
                form.Reading("Stencil buffer", r.stencilAvailable ? "available" : "missing: no outlines");
                form.Reading("Nameplates", std::to_string(r.nameplates));
                form.Reading("Drawn", r.drewInScene ? "into the scene" : "on top");
                form.Reading("Game's names",
                    r.hooked ? Fmt("%u, %u by HeadsUp, %u hidden", r.names, r.replaced, r.hidden) : "not hooked");
                form.Reading("Frame time",
                    Fmt("%.2f ms (%.0f fps)", r.frameMs, r.frameMs > 0.0 ? 1000.0 / r.frameMs : 0.0));
                form.Reading("Last save",
                    Fmt("%.0f ms, %zu %s", r.lastSaveMs, r.lastSaveKeys, r.lastSaveKeys == 1 ? "key" : "keys"));
                form.Reading("Your level",
                    r.level > 0 ? Fmt("%d%s", r.level, r.sitting ? ", sitting" : "") : std::string("unknown"));
                form.EndCard();
            }
        }

        // Each group: its name to type, its Under Bar switch and offset, a remove button, then its windows as chips a
        // click takes out and a dropdown that adds one no group has.
        void WindowGroups(Form& form, Settings& s)
        {
            form.Note("Windows in a group move by its offset together, keeping their places beside each other. Each "
                      "window's own X and Y below still moves it within the group.");
            std::optional<size_t> removed;
            for (size_t g = 0; g < s.windowGroups.size(); ++g)
            {
                WindowGroup& group     = s.windowGroups[g];
                const std::string name = "group " + std::to_string(g);
                form.Line("line group");
                std::string typed = group.name;
                if (form.TextAt((name + " name").c_str(), typed, nullptr, kGroupNameWidth, false, kMaxGroupName) &&
                    !typed.empty())
                {
                    group.name   = typed;
                    form.changed = true;
                }
                form.Raw("<div class=\"gap\"></div>");
                form.CheckAt((name + " under").c_str(), group.underBar, "Under Bar: the whole group goes below the help bar.");
                form.NumberAt("X", (name + " x").c_str(), group.x, kMaxWindowOffset);
                form.NumberAt("Y", (name + " y").c_str(), group.y, kMaxWindowOffset);
                if (form.Button("x", (name + " remove").c_str(), "btn raised round",
                        "Removes the group; its windows stay where the game puts them."))
                    removed = g;
                form.EndLine();
                // Its windows, then the one to add.
                form.Line("line chips indent");
                std::optional<size_t> taken;
                for (size_t w = 0; w < group.windows.size(); ++w)
                    if (form.Button((WindowName(group.windows[w]) + "  x").c_str(),
                            (name + " member " + std::to_string(w)).c_str(), kChip))
                        taken = w;
                std::vector<std::string> free;
                for (const std::string& window : AllWindows())
                    if (std::ranges::none_of(s.windowGroups, [&](const WindowGroup& other) {
                            return std::ranges::find(other.windows, window) != other.windows.end();
                        }))
                        free.push_back(window);
                int chosen = 0;
                if (form.ListAt((name + " add").c_str(), "+ Add Window", kControlWidth * 0.8f, static_cast<int>(free.size()),
                        [&](int i) { return WindowName(free[static_cast<size_t>(i)]); }, -1, chosen))
                {
                    group.windows.push_back(free[static_cast<size_t>(chosen)]);
                    form.changed = true;
                }
                form.EndLine();
                if (taken)
                {
                    group.windows.erase(group.windows.begin() + static_cast<std::ptrdiff_t>(*taken));
                    form.changed = true;
                }
            }
            if (removed)
            {
                s.windowGroups.erase(s.windowGroups.begin() + static_cast<std::ptrdiff_t>(*removed));
                form.changed = true;
            }
            if (s.windowGroups.size() < kMaxWindowGroups && form.Action("+ New Group"))
            {
                s.windowGroups.push_back(WindowGroup{"Group " + std::to_string(s.windowGroups.size() + 1), 0, 0, false, {}});
                form.changed = true;
            }
        }

        // The game's windows HeadsUp moves out from under the help bar, and those it has seen there.
        void MovedWindows(Form& form, Settings& s, const MenuReadings& readings, WindowFilter& list)
        {
            form.Switch("Every Window", s.allUnderBar,
                "Every window that opens under the bar goes below it, not only those switched on or grouped.");
            form.Switch("Game Help", s.moveGameHelp,
                "The game's own help bar goes under the top panel, and the windows under it. Off while it is hidden.");
            form.Line();
            if (form.Button("HeadsUp Defaults", "headsup defaults", kRaisedPill,
                    "Every window under the bar, the game's help too, in the groups the game opens them in. Offsets are "
                    "cleared."))
            {
                UseHeadsUpWindowDefaults(s);
                form.changed = true;
            }
            if (form.Button("HeadsUp Help", "headsup help", kRaisedPill,
                    "HeadsUp's help text in place of the game's help bar, which is hidden, and every window under it."))
            {
                UseHelpTextDefaults(s);
                form.changed = true;
            }
            if (form.Button("Game Defaults", "game defaults", kFlatPill,
                    "Every window where the game puts it: nothing moved, no groups, no offsets."))
            {
                UseGameWindowDefaults(s);
                form.changed = true;
            }
            form.EndLine();
            form.Note("Under Bar takes a window below the help bar, all together; X and Y move it from where the game "
                      "puts it. Type a number, then Enter.");
            {
                form.Row("Find", nullptr);
                form.TextAt("find window", list.text, "Name", kFilterWidth, true, 31);
                // Which windows are listed is the window's state, and no setting to save.
                const bool was = form.changed;
                form.CheckAt("all windows", list.all, "Every window the game has, not only the named ones and yours.");
                form.changed = was;
                form.EndRow();
            }
            // The named windows and the player's, or every one, less those the filter leaves out: in one order
            // whatever is switched, so a row stays put when it changes.
            std::vector<std::string> listed;
            const auto has = [](const auto& names, const std::string& name) {
                return std::ranges::find(names, name) != names.end();
            };
            for (const std::string& name : AllWindows())
            {
                const char* label = WindowLabel(name);
                if (!(list.all || label != nullptr || has(s.movedWindows, name) ||
                        std::ranges::any_of(s.windowOffsets, [&](const WindowOffset& o) { return o.name == name; }) ||
                        (readings.seenWindows != nullptr && has(*readings.seenWindows, name))))
                    continue;
                if (Matches(name, list.text.c_str()) || (label != nullptr && Matches(label, list.text.c_str())))
                    listed.push_back(name);
            }
            for (const std::string& window : listed)
            {
                const auto at     = std::ranges::find(s.movedWindows, window);
                bool moved        = at != s.movedWindows.end();
                const bool was    = moved;
                const char* label = WindowLabel(window);
                form.Row(((label != nullptr ? std::string(label) : window) + "##window " + window).c_str(),
                    "Under Bar, then its X and Y offset.");
                form.CheckAt(("under " + window).c_str(), moved);
                auto offset = std::ranges::find(s.windowOffsets, window, &WindowOffset::name);
                int dx = offset != s.windowOffsets.end() ? offset->x : 0, dy = offset != s.windowOffsets.end() ? offset->y : 0;
                const int oldX = dx, oldY = dy;
                form.NumberAt("X", ("x " + window).c_str(), dx, kMaxWindowOffset);
                form.NumberAt("Y", ("y " + window).c_str(), dy, kMaxWindowOffset);
                form.EndRow();
                if (moved != was)
                {
                    if (moved)
                        s.movedWindows.push_back(window);
                    else
                        s.movedWindows.erase(std::ranges::find(s.movedWindows, window));
                }
                if (dx == oldX && dy == oldY) continue;
                offset = std::ranges::find(s.windowOffsets, window, &WindowOffset::name);
                if (offset == s.windowOffsets.end())
                    s.windowOffsets.push_back(WindowOffset{window, dx, dy});
                else
                {
                    offset->x = dx;
                    offset->y = dy;
                }
            }
        }

        void UserInterfacePage(Form& form, Settings& s, const MenuReadings& readings, MenuFormState& state)
        {
            form.Fade(false);
            // First: the cards under it list every window and run long.
            if (form.BeginCard("Screenshot Mode", nullptr,
                    "Every name, the top panel and every addon window go, as with /hu screenshot. Ashita's /screenshot "
                    "hide turns it on by itself for its picture."))
            {
                std::vector<uint8_t> key;
                if (s.screenshotKey != 0) key.push_back(static_cast<uint8_t>(s.screenshotKey));
                Keys(form, "Key", "Turns screenshot mode on and off. The game sees the key too, so pick one it does not use.",
                    key, 1, state.shotKeyCapture);
                s.screenshotKey = key.empty() ? 0 : key.front();
                form.Switch("Hide game windows", s.screenshotHidesGame, "The game's own windows and chat go too.");
                form.EndCard();
            }
            if (form.BeginCard("Window Groups"))
            {
                WindowGroups(form, s);
                form.EndCard();
            }
            if (form.BeginCard("Game Windows"))
            {
                MovedWindows(form, s, readings, state.windows);
                form.EndCard();
            }
        }

        void TopPanelPage(Form& form, Settings& s, const MenuReadings& readings, const std::vector<std::string>& fonts)
        {
            form.Fade(!s.enabled);
            if (form.BeginCard("Top Panel", &s.showBar,
                    "Hover a widget for details; click it to keep them open. The Widgets page has each one's options."))
            {
                form.Fade(!BarOn(s));
                int theme = static_cast<int>(s.barTheme);
                form.NumberDropdown("Theme", theme, 0, kBarThemeCount - 1,
                    [](int t) { return std::string(BarThemeName(static_cast<BarTheme>(t))); }, nullptr, 0.0f,
                    [](int t) { return std::string(BarThemeTip(static_cast<BarTheme>(t))); });
                s.barTheme = static_cast<BarTheme>(theme);
                const char* const separators[kBarSeparatorsCount] = {"Theme's", "Diamonds", "None"};
                form.Segments("Separators", s.barSeparators, separators, kBarSeparatorsCount, nullptr, kWideSegmentWidth);
                form.FontDropdown("Title font", s.barTitleFont, fonts, nullptr, kThemeFont);
                form.FontDropdown("Text font", s.barTextFont, fonts, nullptr, kThemeFont);
                form.SliderInt("Height", s.barHeight, kMinBarHeight, kMaxBarHeight, "%d px", nullptr);
                std::string textSize = "%d px";
                if (s.barTextSize == kAutoBarTextSize)
                    textSize = "Auto";
                else if (readings.barTitlePixels > 0 && readings.barTitlePixels != s.barTextSize)
                    textSize = Fmt("%%d px (%d fits)", readings.barTitlePixels);
                form.SliderInt("Text size", s.barTextSize, kAutoBarTextSize, kMaxBarTextSize, textSize.c_str(),
                    "Auto follows the height. Too large for two lines, the second line goes as One Line says.");
                const char* const oneLine[kSubtitlePlaceCount] = {"Hide", "Before", "After"};
                form.Segments("One line", s.barSubtitle, oneLine, kSubtitlePlaceCount,
                    "On a bar too short for two lines, each widget's second line goes before or after its title, or is "
                    "left off.");
                form.SliderInt("Popup size", s.popupTextSize, kAutoPopupTextSize, kMaxBarTextSize,
                    s.popupTextSize == kAutoPopupTextSize ? "Auto" : "%d px", "Auto follows the height.");
                form.Fade(!s.enabled);
                form.Switch("Hide readout", s.hideNetstat, "The game's S, R and % numbers at the top right.");
                form.EndCard();
            }
            const int help = static_cast<int>(BarWidget::GameHelp);
            form.Fade(!BarOn(s));
            if (form.BeginCard("Help Bar", &s.barWidgets[help], kBarWidgetTips[help]))
            {
                form.Fade(!s.enabled);
                form.Switch("Hide game help", s.hideHelpBar, "The game's own help bar, which the top panel covers.");
                form.Fade(!BarOn(s) || !s.barWidgets[help]);
                form.Offset("Position", s.helpBarX, s.helpBarY, kMaxHelpBarOffset,
                    "Pixels from its place under the top panel.");
                form.Switch("Background", s.helpBarBackground, "Off, the help text alone, with no bar behind it.");
                form.EndCard();
            }
            form.Fade(false);
        }

        // One widget's options: its switch, its section, what it shows and its own settings.
        void WidgetOptions(Form& form, Settings& s, BarWidget widget)
        {
            const int w = static_cast<int>(widget);
            form.Note(kBarWidgetTips[w]);
            // Each widget's controls have ids of their own: nothing open or typed in is carried to another tab.
            const std::string scope = std::string("##") + BarWidgetName(widget);
            const auto label        = [&scope](const char* text) { return text + scope; };
            form.Switch(label("Enabled").c_str(), s.barWidgets[w], nullptr);
            form.Fade(!BarOn(s) || !s.barWidgets[w]);
            auto [section, index] = PlaceOf(s.barSections, widget);
            int moved             = section;
            form.Segments(label("Section").c_str(), moved, kBarSectionLabels, kBarSectionCount, nullptr);
            if (moved != section)
                form.changed |= MoveWidget(s.barSections, static_cast<BarSection>(section), index,
                    static_cast<BarSection>(moved), s.barSections[moved].size());
            if (HasSecondLine(widget)) form.Switch(label("Second line").c_str(), s.barSecondLine[w], nullptr);
            if (HasIcons(widget)) form.Switch(label("Icons").c_str(), s.barIcons[w], nullptr);
            switch (widget)
            {
            case BarWidget::Target:
                form.Switch("Details", s.barTargetDetails, "Aggro icons after the name, weaknesses after the level.");
                break;
            case BarWidget::Gil:
                form.NumberDropdown("Currency", s.barCurrency, kGilCurrency, kCurrencyCount - 1,
                    [](int c) { return std::string(kCurrencyTable[c].name); }, nullptr);
                break;
            case BarWidget::EarthTime:
            {
                int zone = ClockZoneChoice(s.clockZone);
                form.NumberDropdown("Time zone", zone, 0, kClockZoneChoices - 1, ClockZoneChoiceName,
                    "Local is your computer's. Another zone goes before the time, as in PDT 21:07.");
                s.clockZone = ClockZoneOfChoice(zone);
                form.Switch("Server time", s.clockServerTime,
                    "Japan's time, which the server keeps and the resets go by, as a second line under your own.");
                form.Switch("24-hour", s.clock24, nullptr);
                form.Switch("Seconds", s.clockSeconds, "On your own line.");
                break;
            }
            case BarWidget::Job:
            {
                const char* const layouts[kJobLayoutCount] = {"Jobs first", "EXP first", "EXP only"};
                form.Segments("Layout", s.jobLayout, layouts, kJobLayoutCount,
                    "Which is the larger line, your jobs and levels or your EXP, with the other under it; or the EXP "
                    "alone.",
                    kWideSegmentWidth);
                const char* const places[kExpBarPlaceCount] = {"Under", "Beside", "Off"};
                form.Segments("EXP bar", s.expBar, places, kExpBarPlaceCount, nullptr);
                form.Switch("Job names", s.jobNames, "Off, the levels alone.");
                break;
            }
            case BarWidget::Connection: form.Switch("Latency", s.barLatency, "After the rates, once measured."); break;
            default: break;
            }
        }

        // Left, Center and Right as trays sunk into the card, each holding its widgets as raised chips in bar order:
        // a chip drags to another place or tray, and a click picks it, which opens its tab. A lit dot says it is on.
        // Returns whether a chip was clicked.
        bool Trays(Form& form, Settings& s, int& picked)
        {
            struct Drop
            {
                BarSection from;
                size_t index;
                BarSection to;
                size_t at;
            };
            bool clicked = false;
            std::optional<Drop> drop; // applied after the trays are written, so no list changes under its own loop
            const std::string chipStart = form.Named(kWidgetChip) + "-";
            // The chip a drop names, as the section it is in and its place there.
            const auto dropped = [&](const std::string& target, BarSection to, size_t at) {
                const FormEvent* event = form.Took(target, kDropEvent);
                if (event == nullptr || event->value.compare(0, chipStart.size(), chipStart) != 0) return;
                const int w = std::atoi(event->value.c_str() + chipStart.size());
                if (w < 0 || w >= kBarWidgetCount || !OnTheBar(static_cast<BarWidget>(w))) return;
                const auto [section, index] = PlaceOf(s.barSections, static_cast<BarWidget>(w));
                drop = Drop{static_cast<BarSection>(section), index, to, at};
            };
            form.Line("line trays");
            for (int section = 0; section < kBarSectionCount; ++section)
            {
                const auto here = static_cast<BarSection>(section);
                form.Raw(std::string("<div class=\"tray\"><div class=\"tray-name\">") + kBarSectionLabels[section] + "</div>");
                const std::vector<BarWidget>& widgets = s.barSections[section];
                for (size_t i = 0; i < widgets.size(); ++i)
                {
                    const int w          = static_cast<int>(widgets[i]);
                    const std::string id = form.Id(kWidgetChip + std::to_string(w));
                    form.Raw("<div class=\"btn raised wchip drag\" id=\"" + id + "\" tip=\"" + EscapedRml(kBarWidgetTips[w]) +
                             "\"><div class=\"dot\"></div>" + EscapedRml(TitleCaseText(kBarWidgetLabels[w])) + "</div>");
                    form.Set(id, FormValueKind::Class, "off", {}, form.Off());
                    if (!form.Off() && form.Clicked(id))
                    {
                        picked  = w;
                        clicked = true;
                    }
                    form.Set(id, FormValueKind::Class, "on", {}, s.barWidgets[w]);
                    form.Set(id, FormValueKind::Class, "picked", {}, w == picked);
                    dropped(id, here, i); // dropped on a chip: before it
                }
                // The rest of the tray takes a drop at the section's end.
                const std::string end = form.Id("tray end " + std::to_string(section));
                form.Raw("<div class=\"tray-end\" id=\"" + end + "\"></div></div>");
                dropped(end, here, widgets.size());
            }
            form.EndLine();
            if (drop) form.changed |= MoveWidget(s.barSections, drop->from, drop->index, drop->to, drop->at);
            return clicked;
        }

        void WidgetsPage(Form& form, Settings& s, MenuFormState& state)
        {
            // The tabs come first, so a tab's options show without scrolling past the trays.
            form.Fade(!BarOn(s));
            if (form.BeginCard("Widgets", nullptr, "A tab for each widget, with its options."))
            {
                const std::vector<BarWidget> tabs = WidgetTabs();
                std::vector<std::string> names;
                std::vector<const char*> labels;
                names.reserve(tabs.size()); // the labels point into it
                for (const BarWidget widget : tabs)
                    labels.push_back(names.emplace_back(TitleCaseText(kBarWidgetLabels[static_cast<int>(widget)])).c_str());
                const auto open = std::ranges::find(tabs, static_cast<BarWidget>(state.widget));
                const int count = static_cast<int>(tabs.size());
                const int tab   = form.Tabs("widget", open != tabs.end() ? static_cast<int>(open - tabs.begin()) : 0,
                    labels.data(), count, TabsPerRow(count, kWidgetTabRows));
                state.widget    = static_cast<int>(tabs[static_cast<size_t>(tab)]);
                WidgetOptions(form, s, tabs[static_cast<size_t>(tab)]);
                form.EndCard();
            }
            form.Fade(!BarOn(s));
            if (form.BeginCard("Layout", nullptr, "Drag a chip to move it; click it to open its tab above."))
            {
                // A clicked chip's tab is at the page's top, which the trays may have scrolled out of view.
                if (Trays(form, s, state.widget)) form.scrollToTop = true;
                if (form.Action("Reset Layout", "Puts every widget back in its first place. Switches stay.") &&
                    s.barSections != DefaultPlacement())
                {
                    s.barSections = DefaultPlacement();
                    form.changed  = true;
                }
                form.EndCard();
            }
            form.Fade(false);
        }

        // entryTip, when given, says for an entry when it applies.
        template <typename Entry>
        void PaletteCard(Form& form, const char* title, const char* tip, Color* colors, int count, const char* (*label)(Entry),
            const char* (*entryTip)(Entry) = nullptr)
        {
            if (!form.BeginCard(title, nullptr, tip)) return;
            // Two columns, the first the longer.
            const int first = (count + 1) / 2;
            form.Raw("<div class=\"columns\"><div class=\"column\">");
            for (int i = 0; i < count; ++i)
            {
                if (i == first) form.Raw("</div><div class=\"column second\">");
                const auto entry = static_cast<Entry>(i);
                form.Swatch((std::string(label(entry)) + "##" + title).c_str(), colors[i],
                    entryTip != nullptr ? entryTip(entry) : nullptr);
            }
            form.Raw("</div></div>");
            form.EndCard();
        }

        void Visibility(Form& form, Settings& s, PlateKind kind, const int* order, int rows)
        {
            KindSettings& k   = s.Kind(kind);
            const bool plates = s.enabled && s.nameplates, outlines = s.enabled && s.outlines;
            form.Fade(!s.enabled);
            if (!form.BeginCard("Visibility", nullptr, "What each kind of entity shows, and whether it gets an outline."))
                return;
            form.Row("##visibility heads", nullptr);
            form.Fade(!plates);
            form.Caption("Nameplates", "head", kModeWidth);
            form.Fade(!outlines);
            form.Caption("Outlines", "head", kModeWidth);
            form.EndRow();
            for (int i = 0; i < rows; ++i)
            {
                const int r      = order[i];
                RowSettings& row = k.rows[r];
                form.Fade(!s.enabled);
                form.Row(RowLabel(kind, r), RowTip(kind, r));
                form.Fade(!plates);
                int plate = static_cast<int>(row.plate);
                form.DropdownAt((std::string("plate ") + RowKey(kind, r)).c_str(), plate, 0, kPlateModeCount - 1,
                    [](int m) { return std::string(PlateModeLabel(static_cast<PlateMode>(m))); }, kModeWidth, nullptr,
                    [](int m) { return std::string(PlateModeTip(static_cast<PlateMode>(m))); });
                row.plate = static_cast<PlateMode>(plate);
                form.Fade(!outlines);
                int outline         = static_cast<int>(row.outline);
                const bool claimRow = kind == PlateKind::Mobs && IsClaimRow(static_cast<Category>(r));
                form.DropdownAt((std::string("outline ") + RowKey(kind, r)).c_str(), outline, 0, OutlineModesFor(kind, r) - 1,
                    [](int m) { return std::string(OutlineModeLabel(static_cast<OutlineMode>(m))); }, kModeWidth, nullptr,
                    [claimRow](int m) { return std::string(OutlineModeTip(static_cast<OutlineMode>(m), claimRow)); });
                row.outline = static_cast<OutlineMode>(outline);
                form.EndRow();
            }
            form.EndCard();
        }

        void OutlineDesign(Form& form, Settings& s, PlateKind kind, const int* order, int rows)
        {
            KindSettings& k = s.Kind(kind);
            form.Fade(!s.enabled || !s.outlines);
            if (!form.BeginCard("Outline Design", nullptr,
                    "Each row's outline color: a custom color, or one of the palettes by the entity's state."))
                return;
            const std::vector<ColorSource> sources = SourcesFor(kind, false);
            for (int i = 0; i < rows; ++i)
            {
                RowSettings& row = k.rows[order[i]];
                if (!HasOutlineColor(row.outline)) continue;
                form.ColorPickRow((std::string(RowLabel(kind, order[i])) + "##outline").c_str(), row.outlineColor, sources,
                    "This row's outline color: your own, or a palette on the Entities page.");
            }
            form.EndCard();
        }

        void NameplateDesign(Form& form, Settings& s, PlateKind kind, DesignEditor& editor)
        {
            KindSettings& k = s.Kind(kind);
            form.Fade(!s.enabled || !s.nameplates);
            if (!form.BeginCard("Nameplate Design", nullptr,
                    "Design this entity's nameplate by writing in the text box and by dragging and dropping tags from below."))
                return;
            for (int i = 0; i < kPlateLines; ++i)
            {
                form.Line("line design");
                form.Caption(kLineLabels[i], "line-name");
                if (form.LineBox((std::string("design ") + KindKey(kind) + " " + std::to_string(i)).c_str(), k.lines[i].text,
                        editor.drafts[i]) ||
                    editor.drafts[i].typing)
                    editor.line = i;
                form.EndLine();
            }
            form.Line("line chips");
            for (const Tag tag : TagsFor(kind))
                if (form.TagChip(tag) && PutTag(k.lines[editor.line].text, editor.drafts[editor.line].caret, tag))
                    form.changed = true;
            form.EndLine();
            // Under the chips, so nothing above moves under the mouse when it comes and goes.
            if (const std::string note = UnusedTagsNote(k, {Tag::Name}, kShowsNameEnding); !note.empty())
                form.Note(note.c_str());
            form.EndCard();
        }

        // Every tab shows the same rows; a synced line's tab alone shows only its Sync row.
        void LineStyles(Form& form, Settings& s, PlateKind kind, const std::vector<std::string>& fonts, DesignEditor& editor)
        {
            KindSettings& k = s.Kind(kind);
            const bool off  = !s.enabled || !s.nameplates;
            form.Fade(off);
            if (!form.BeginCard("Line Style", nullptr, "How each line is drawn. A line's tab holds its settings.")) return;
            editor.styleTab  = form.Tabs("line style", editor.styleTab, kLineLabels, kPlateLines);
            const int n      = editor.styleTab;
            LineDesign& line = k.lines[n];
            // Each line's controls have ids of their own: one held, open or typed in is not carried to another tab.
            const std::string scope = std::string("##") + KindKey(kind) + std::to_string(n);
            const auto label        = [&scope](const char* text) { return text + scope; };

            const std::vector<int> targets = SyncTargets(k, n);
            const int synced               = SyncEntry(targets, line.syncWith);
            int sync                       = synced;
            form.NumberDropdown(label("Sync Line Style").c_str(), sync, 0, static_cast<int>(targets.size()),
                [&targets](int entry) { return SyncEntryLabel(targets, entry); },
                "Borrows every setting below from another line, so the two always match. A line that borrows from this "
                "one is not offered.",
                0.0f, [&targets](int entry) { return SyncEntryTip(targets, entry); });
            if (sync != synced) line.syncWith = SyncOfEntry(targets, sync);
            if (line.syncWith != kNoSync)
            {
                form.Note((std::string("Uses the ") + kLineLabels[line.syncWith] + "'s style.").c_str());
                form.EndCard();
                return;
            }
            const std::vector<ColorSource> sources = SourcesFor(kind, true);
            form.ChecksDropdown(label("Line Visibility").c_str(), line.show, ConditionsFor(kind),
                "When this line shows: always, or when any checked condition holds.");
            // The bottom line is what the others are set against.
            if (n < kPlateLines - 1)
                form.Segments(label("Alignment").c_str(), line.align, kAlignLabels, kAlignCount,
                    "Where the line sits against the bottom line. A line wider than it hangs past its other end.",
                    kControlWidth / kAlignCount);
            form.FontDropdown(label("Font").c_str(), line.font, fonts,
                "The line's font. Default is the font on the Entities page.", kDefaultFontChoice, &line.bold);
            form.SliderInt(label("Text Size").c_str(), line.textSize, kMinSizePercent, kMaxSizePercent, "%d%%",
                "Of the base font size on the Entities page.");
            form.SliderInt(label("Icon Size").c_str(), line.iconSize, kMinSizePercent, kMaxSizePercent, "%d%%",
                "Every icon on this line, of the base font size.");
            form.ColorPickRow(label("Text Color").c_str(), line.color, sources,
                "The text's color: your own, the game's, or a palette by the entity's state.");
            form.SliderInt(label("Shadow Strength").c_str(), line.shadow, kMinShadow, kMaxShadow, "%d%%",
                "The dark, soft shadow behind the letters that keeps them readable over bright ground. 0 is none.");
            form.Fade(off || line.shadow == 0);
            form.ColorPickRow(label("Shadow Color").c_str(), line.shadowColor, sources, "The shadow's color; black as a rule.");
            form.Fade(off);
            form.SliderInt(label("Glow Strength").c_str(), line.glow, kMinGlow, kMaxGlow, "%d%%",
                "A colored halo around the letters, as a mob's name glows in its outline color. 0 is none.");
            form.Fade(off || line.glow == 0);
            form.SliderInt(label("Glow Size").c_str(), line.glowSize, kMinGlowSize, kMaxGlowSize, "%d%%",
                "How far the halo spreads.");
            form.ColorPickRow(label("Glow Color").c_str(), line.glowColor, sources, "The halo's color.");
            form.EndCard();
        }

        void Ornament(Form& form, Settings& s, PlateKind kind)
        {
            OrnamentSettings& o = s.Kind(kind).ornament;
            const bool off      = !s.enabled || !s.nameplates;
            form.Fade(off);
            if (!form.BeginCard("Ornament", &o.on, "A diamond rule between two of the lines, drawn while both show.")) return;
            form.Fade(off || !o.on);
            const std::string scope = std::string("##") + KindKey(kind) + " ornament";
            form.Segments(("Position" + scope).c_str(), o.place, kOrnamentPlaceLabels, kOrnamentPlaceCount,
                "Which two lines it sits between. It shows only while both do.", kControlWidth / kOrnamentPlaceCount);
            form.SliderInt(("Width" + scope).c_str(), o.width, kMinOrnamentWidth, kMaxOrnamentWidth, "%d px",
                "The rule's length, before the distance scale.");
            form.SliderInt(("Height" + scope).c_str(), o.height, kMinOrnamentThickness, kMaxOrnamentThickness, "%d px",
                "The diamond's height.");
            form.ColorPickRow(("Color" + scope).c_str(), o.color, SourcesFor(kind, false),
                "Its color: your own, or a palette by the entity's state.");
            form.EndCard();
        }

        void MobCards(Form& form, Settings& s, const std::vector<std::string>& fonts)
        {
            const bool off = !s.enabled || !s.nameplates;
            // A card whose settings show only through tags says which, while no line of the design holds one of them.
            const auto shown = [&](std::initializer_list<Tag> tags) {
                const std::string note = UnusedTagsNote(s.Kind(PlateKind::Mobs), tags, kShowsTheseEnding);
                if (!note.empty()) form.Note(note.c_str());
                return note.empty();
            };
            form.Fade(off);
            if (form.BeginCard("Fading"))
            {
                form.SliderInt("Others' Opacity", s.untargetedOpacity, kMinOpacity, kMaxOpacity, "%d%%",
                    "Other mobs' and pets' names and lines while you have a target, so your target's stand out.");
                form.Switch("Glow Off While Fighting", s.glowOffWhenFighting,
                    "No glow on a mob's lines once you or your party has claimed it.");
                form.EndCard();
            }
            if (form.BeginCard("Detection Icons", nullptr,
                    "Which icons the Detection tag shows: aggression, linking and how it senses you, in MobDB's icons."))
            {
                form.Fade(off || !shown({Tag::Detect}));
                for (int d = 0; d < kDetectionIconCount; ++d)
                {
                    // Aggression and links first; the rest is what the mob senses you by.
                    if (static_cast<DetectionIcon>(d) == DetectionIcon::Sight)
                    {
                        form.Caption("Senses:", "line-name");
                        form.Indent(true);
                    }
                    const uint32_t bit = DetectionBit(static_cast<DetectionIcon>(d));
                    bool on            = (s.detectionShown & bit) != 0;
                    form.Switch(DetectionLabel(static_cast<DetectionIcon>(d)), on, nullptr, "Hide", "Show");
                    s.detectionShown = on ? s.detectionShown | bit : s.detectionShown & ~bit;
                }
                form.Indent(false);
                form.EndCard();
            }
            // The Target widget's popup reads the ranks switch and these colors too, whatever the nameplates do or hold.
            form.Fade(!s.enabled);
            if (form.BeginCard("Badges", nullptr,
                    "The small text on the icons of the Weaknesses, Resistances, Statuses and Immunities tags. The Target "
                    "widget's popup takes its ranks and colors from here too."))
            {
                shown({Tag::Weak, Tag::Resist, Tag::Status, Tag::Immune});
                form.FontDropdown("Badge Font", s.badgeFontName, fonts);
                form.Switch("Use Rank System", s.showRanks,
                    "Phoenix's ranks, -3 to 11, instead of percents. -3 is +50% and 4 is -50%.");
                form.Swatch("Weak Color", s.weakBadgeColor, nullptr);
                form.Swatch("Resist Color", s.resistBadgeColor, nullptr);
                form.Swatch("Status Resist Color", s.rankBadgeColor, nullptr);
                form.Swatch("Immune Color", s.immuneBadgeColor, nullptr);
                form.EndCard();
            }
            const auto cutoff    = [&s](int rank) { return CutoffText(rank, s.showRanks); };
            const auto weakTip   = [&s](int rank) { return CutoffTip(rank, s.showRanks, true); };
            const auto resistTip = [&s](int rank) { return CutoffTip(rank, s.showRanks, false); };
            form.Fade(off);
            if (form.BeginCard("Weaknesses", nullptr,
                    "Which elements the Weaknesses and Resistances tags show beside the damage types."))
            {
                form.Fade(off || !shown({Tag::Weak, Tag::Resist}));
                form.NumberDropdown("Weak From", s.elementWeakRank, kMaxWeakRank, kNoWeakRank, cutoff,
                    "The least element weakness that gets a badge.", 0.0f, weakTip);
                form.NumberDropdown("Resist From", s.elementResistRank, kMinResistRank, kNoResistRank, cutoff,
                    "The least element resistance that gets a badge.", 0.0f, resistTip);
                form.NumberDropdown("Max Elements", s.maxElements, kElementCount, 0, ElementCountText,
                    "Past this many, those nearest neutral are left off.", 0.0f, ElementCountTip);
                form.EndCard();
            }
            form.Fade(off);
            if (form.BeginCard("Statuses", nullptr, "Which statuses the Statuses tag shows, weak or resisted."))
            {
                const bool dim = off || !shown({Tag::Status});
                form.Fade(dim);
                form.Row("##status heads", nullptr);
                form.Caption("", "head", kToggleWidth);
                form.Caption("Weak", "head", kCutoffWidth);
                form.Caption("Resist", "head", kCutoffWidth);
                form.EndRow();
                for (int i = 0; i < kStatusRankCount; ++i)
                {
                    StatusBadgeRule& rule = s.statusBadges[i];
                    const std::string status = StatusRankLabel(static_cast<StatusRank>(i));
                    form.Fade(dim);
                    form.Row(status.c_str(), nullptr);
                    form.Raw("<div class=\"switch-cell\">");
                    form.CheckAt(("status " + status).c_str(), rule.show);
                    form.Raw("</div>");
                    form.Fade(dim || !rule.show);
                    form.DropdownAt(("weak " + status).c_str(), rule.weakRank, kMaxWeakRank, kNoWeakRank, cutoff, kCutoffWidth,
                        nullptr, weakTip);
                    form.DropdownAt(("resist " + status).c_str(), rule.resistRank, kMinResistRank, kNoResistRank, cutoff,
                        kCutoffWidth, nullptr, resistTip);
                    form.EndRow();
                }
                form.EndCard();
            }
        }

        void Friends(Form& form, Settings& s, std::string& typed)
        {
            form.Fade(!s.enabled);
            if (!form.BeginCard("Friends", nullptr,
                    "Players named here are Friends, in the rows above and in the Entity Type colors. Click a name to "
                    "remove it."))
                return;
            form.Line("line chips");
            std::optional<size_t> removed;
            for (size_t i = 0; i < s.friends.size(); ++i)
                if (form.Button((s.friends[i] + "  x").c_str(), ("friend " + std::to_string(i)).c_str(), kChip)) removed = i;
            const std::string rule = "A character's name, " + std::to_string(kMinCharacterName) + " to " +
                                     std::to_string(kMaxCharacterName) + " letters. Enter adds it.";
            std::string name = typed;
            form.Raw("<div class=\"tip-wrap\" tip=\"" + EscapedRml(rule) + "\">");
            const bool entered = form.TextAt("add friend", name, "Add a Name", kFilterWidth, false, kMaxCharacterName);
            form.Raw("</div>");
            form.EndLine();
            if (entered)
            {
                typed = name;
                if (std::optional<std::string> added = FriendToAdd(s.friends, typed))
                {
                    s.friends.push_back(std::move(*added));
                    typed.clear();
                    form.changed = true;
                }
            }
            if (removed)
            {
                s.friends.erase(s.friends.begin() + static_cast<std::ptrdiff_t>(*removed));
                form.changed = true;
            }
            form.EndCard();
        }

        void Timers(Form& form, Settings& s)
        {
            form.Fade(!s.enabled);
            if (!form.BeginCard("Placeholder Timers")) return;
            form.Switch("Respawn Timers", s.phTimers,
                "Above your name, a countdown for each placeholder you see die, as Bigmouth Billy [11A] 3:15. It reads "
                "\"up\" when due.");
            form.Fade(!s.enabled || !s.phTimers);
            form.SliderInt("Timer Size", s.timerPercent, kMinSizePercent, kMaxSizePercent, "%d%%",
                "Of the base font size on the Entities page.");
            form.EndCard();
        }

        void EntitiesPage(Form& form, Settings& s, const MenuReadings& readings, const std::vector<std::string>& fonts)
        {
            form.Fade(!s.enabled);
            if (form.BeginCard("Nameplates", &s.nameplates,
                    "HeadsUp's nameplates: names, levels, icons and the rest, as each kind's page designs them."))
            {
                form.Fade(!s.enabled || !s.nameplates);
                form.SliderInt("Base Font Size", s.baseSize, kMinTextSize, kMaxTextSize, "%d px",
                    "Every line's text and icon size is a percent of this.");
                form.FontDropdown("Default Font", s.defaultFont, fonts, "The font of every line whose own font is Default.");
                form.Switch("Scale with Distance", s.scaleWithDistance, "Sizes follow how far a name's owner is from you.");
                form.Fade(!s.enabled || !s.nameplates || !s.scaleWithDistance);
                form.Indent(true);
                form.SliderInt("Min Size", s.scaleMin, kMinSizePercent, kMaxSizePercent, "%d%%",
                    "The smallest a far name shrinks to.");
                form.SliderInt("Max Size", s.scaleMax, kMinSizePercent, kMaxSizePercent, "%d%%",
                    "The largest a near name grows to.");
                form.Switch("Scale Your Own Name", s.scaleOwnName,
                    "Off, your own nameplate keeps one size as the camera zooms.");
                form.Indent(false);
                form.Fade(!s.enabled || !s.nameplates);
                form.SliderInt("Vertical Offset", s.nameRaise, kMinNameRaise, kMaxNameRaise, "%d px",
                    "How far above the game's own spot names sit.");
                form.Switch("Stack Names", s.stackNames,
                    "Names that would cover each other rise one over another. Your target's keeps its place.");
                form.EndCard();
            }
            form.Fade(!s.enabled);
            if (form.BeginCard("Outline Style", &s.outlines,
                    "Colored borders around mobs, and anything else whose rows ask for one."))
            {
                form.Fade(!s.enabled || !s.outlines);
                form.SliderFloat("Thickness", s.thickness, kMinThickness, kMaxThickness, "%.1f px", nullptr);
                form.SliderInt("Smoothness", s.smoothness, kMinSmoothness, kMaxSmoothness, "%d copies",
                    "More is rounder and costs a little more.");
                form.SliderFloat("Range", s.maxDistance, kMinOutlineDistance, kMaxOutlineDistance, "%.0f yalms",
                    "Anything farther gets no outline.");
                if (!readings.stencilAvailable) form.Note("Outlines are unavailable: the depth buffer has no stencil bits.");
                form.EndCard();
            }
            form.Fade(!s.enabled);
            if (form.BeginCard("Target Cursor", &s.replaceCursor,
                    "Draws HeadsUp's cursor above the target's nameplate instead of the game's."))
            {
                form.Fade(!s.enabled || !s.replaceCursor);
                form.Switch("Feather", s.cursorFeather, "Phoenix's feather instead of the arrow.");
                form.SliderInt("Size", s.cursorSize, kMinTextSize, kMaxTextSize, "%d px", nullptr);
                form.SliderInt("Outline", s.cursorOutline, kMinCursorOutline, kMaxCursorOutline, "%d%%",
                    "Its dark edge, as a share of the usual thickness.");
                form.Switch("Scale", s.scaleCursor, "Larger the nearer your target is.");
                form.Swatch("Edge Color", s.cursorEdgeColor, "The cursor's dark edge.");
                form.Note("Its colors are the Targeting colors below.");
                form.EndCard();
            }
            form.Fade(!s.enabled);
            PaletteCard(form, "Colors: Aggression", kAggressionColorsTip, s.palette.aggression, kNatureCount, NatureLabel);
            PaletteCard(form, "Colors: Toughness",
                (std::string(kToughnessColorsTip) + " Easy Prey covers Incredibly Easy Prey, and Very Tough covers Incredibly Tough.").c_str(),
                s.palette.toughness, kLabelShadeCount, ToughnessLabel);
            PaletteCard(form, "Colors: Claim Status", kClaimColorsTip, s.palette.claim, kClaimStateCount, ClaimLabel);
            PaletteCard(form, "Colors: Targeting", (std::string(kTargetingColorsTip) + " The target cursor takes these too.").c_str(),
                s.palette.target, kTargetStateCount, TargetStateLabel, TargetStateTip);
            PaletteCard(form, "Colors: Entity Type", kEntityTypeColorsTip, s.palette.type, kTargetTypeCount, TargetTypeLabel);
            form.Fade(false);
        }

        void KindPage(Form& form, Settings& s, PlateKind kind, MenuFormState& state, const std::vector<std::string>& fonts)
        {
            DesignEditor& editor = state.design;
            if (editor.kind != kind)
            {
                editor = DesignEditor{.kind = kind};
                state.friendName.clear();
            }
            const KindSettings& k = s.Kind(kind);
            const int rows        = RowCountOf(kind);
            int order[kMaxRows]   = {}; // the rows as the menu lists them
            bool outlineColors = false, plates = false;
            for (int i = 0; i < rows; ++i)
            {
                order[i] = kind == PlateKind::Mobs ? CategoryIndex(kMobRowOrder[i]) : i;
                outlineColors |= HasOutlineColor(k.rows[order[i]].outline);
                plates |= DrawsPlate(k.rows[order[i]].plate);
            }
            Visibility(form, s, kind, order, rows);
            if (outlineColors) OutlineDesign(form, s, kind, order, rows);
            if (plates)
            {
                NameplateDesign(form, s, kind, editor);
                LineStyles(form, s, kind, fonts, editor);
                Ornament(form, s, kind);
            }
            switch (kind)
            {
            case PlateKind::Mobs: MobCards(form, s, fonts); break;
            case PlateKind::Players: Friends(form, s, state.friendName); break;
            case PlateKind::Self: Timers(form, s); break;
            default: break;
            }
            form.Fade(false);
        }
    }

    void BuildMenu(Form& form, Settings& s, const MenuReadings& readings, MenuFormState& state,
        const std::vector<std::string>& fonts, const std::vector<std::string>& menuFonts)
    {
        Sidebar(form, state);
        form.pageKey = PageKey(state.page);
        switch (state.page)
        {
        case Page::Entities: EntitiesPage(form, s, readings, fonts); break;
        case Page::Mobs:
        case Page::Players:
        case Page::Self:
        case Page::Npcs:
        case Page::Pets: KindPage(form, s, *KindOfPage(state.page), state, fonts); break;
        case Page::UserInterface: UserInterfacePage(form, s, readings, state); break;
        case Page::TopPanel: TopPanelPage(form, s, readings, fonts); break;
        case Page::Widgets: WidgetsPage(form, s, state); break;
        case Page::MouseCursor: MouseCursorPage(form, s, state); break;
        case Page::ConfigMenu: ConfigMenuPage(form, s, state, menuFonts); break;
        case Page::Debug: DebugPage(form, s, readings, state); break;
        }
        form.Fade(false);
        // A page whose every card is switched off says what switches them on.
        if (form.CardsShown() == 0 && form.BeginCard("Switched Off"))
        {
            form.Note(s.enabled ? "These settings are the top panel's, which is off: the Top Panel page switches it on."
                                : "HeadsUp is off: the Debug page switches it on.");
            form.EndCard();
        }
        s = Clamp(s);
    }
}
