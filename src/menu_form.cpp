#include "menu_form.h"

#include "bar_theme.h"
#include "cursor_picture.h"
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
#include <span>
#include <utility>

namespace headsup
{
    namespace
    {
        constexpr const char* kChip        = "btn raised chip";
        constexpr const char* kQuietChip   = "btn flat chip";
        constexpr const char* kResetPill   = "btn wash reset-all"; // a card's width, under the cards
        constexpr const char* kNavStart    = "nav-";
        constexpr const char* kWidgetChip  = "chip "; // a tray chip's name in its card, before its widget's number
        constexpr float kAddWindowShare    = 0.8f;    // of a dropdown's width: a group's "+ Add Window"
        constexpr size_t kMaxFindText      = 31;      // letters of a window's name to look for
        constexpr int kHelpWidget          = static_cast<int>(BarWidget::GameHelp);

        // What a page is drawn with and from.
        struct Context
        {
            Form& form;
            Settings& s;
            const MenuReadings& readings;
            MenuFormState& state;
            const std::vector<std::string>& fonts;
            const std::vector<std::string>& menuFonts;
            const WindowLayout& layout;
            std::optional<PlateKind> kind; // an entity page's
            bool plates        = false;    // the kind has a row that draws a nameplate
            bool outlineColors = false;    // and one outlined in its own color pick
            const std::vector<std::string> plateFonts = NameplateFontChoices(fonts);
        };

        std::string Words(const char* text) { return std::string(Said(text)); }

        // A label that shows name and names its control's id by key, and scope where one control is drawn for
        // several lines or widgets, whatever name says.
        std::string Labeled(const char* name, std::string_view key, std::string_view scope = {})
        {
            return Words(name) + "##" + std::string(key) + std::string(scope);
        }
        const char* Word(const Context& c, WindowWord word) { return c.layout.WordOf(word).name; }
        const char* WordTip(const Context& c, WindowWord word) { return c.layout.WordOf(word).tip; }
        std::string_view FileName(std::string_view path) { return path.substr(path.find_last_of("\\/") + 1); }

        // A tip that says how near a fade begins.
        std::string Near(const char* tip) { return Filled(Said(tip), kNearMark, Formatted("%.0f", kFadeNearYalms)); }

        // A list's names, for the kit's rows of choices and named steps.
        std::vector<const char*> NamesOf(std::span<const LayoutOption> options)
        {
            std::vector<const char*> names;
            for (const LayoutOption& option : options)
                names.push_back(option.name != nullptr ? option.name : "");
            return names;
        }

        // A dropdown of a list's options for a value of the enum they name, in its order.
        template <typename Enum>
        void OptionsDropdown(Form& form, const std::string& label, Enum& value, std::span<const LayoutOption> options,
            const char* tip)
        {
            int picked = static_cast<int>(value);
            form.NumberDropdown(label.c_str(), picked, 0, static_cast<int>(options.size()) - 1,
                [&](int i) { return Words(options[static_cast<size_t>(i)].name); }, tip, 0.0f,
                [&](int i) { return Words(options[static_cast<size_t>(i)].tip); });
            value = static_cast<Enum>(picked);
        }

        // Whether a mob's design holds one of these tags, which the settings of a card show through.
        bool Uses(const Settings& s, std::initializer_list<Tag> tags)
        {
            return std::ranges::any_of(tags, [&](Tag tag) { return UsesTag(s.Kind(PlateKind::Mobs), tag); });
        }
        // What a card says while no line of a design holds one of these tags.
        std::string TagsNote(const Context& c, const KindSettings& kind, std::initializer_list<Tag> tags, WindowWord sentence)
        {
            return UnusedTagsNote(kind, tags, Word(c, sentence), c.layout.OptionsOf(WindowList::Tags), Word(c, WindowWord::Or));
        }

        // The pages as pills, the picked one filled; a child page is indented.
        void Sidebar(Context& c)
        {
            Form& form = c.form;
            form.BeginBlock(kSidebarBlock);
            for (const LayoutPage& page : c.layout.pages)
            {
                const std::string id = kNavStart + std::string(page.key);
                if (form.Clicked(id) && c.state.page != page.page)
                {
                    c.state.Leave();
                    c.state.page     = page.page;
                    form.scrollToTop = true;
                }
                form.Raw(std::string("<div class=\"nav") + (page.child ? " child" : "") + "\" id=\"" + id + "\">" +
                         EscapedRml(page.name) + "</div>");
            }
            for (const LayoutPage& page : c.layout.pages)
                form.Set(kNavStart + std::string(page.key), FormValueKind::Class, kPickedClass, {}, page.page == c.state.page);
        }

        // A row of keys: each a chip that a click removes, and while there are fewer than most, a chip that takes
        // the next key pressed that was not already held.
        void Keys(Context& c, const std::string& label, const char* tip, std::vector<uint8_t>& keys, size_t most,
            KeyCapture& capture)
        {
            Form& form = c.form;
            form.Row(label.c_str(), tip);
            form.EndRow();
            form.Line("line chips");
            std::optional<size_t> removed;
            for (size_t i = 0; i < keys.size(); ++i)
                if (form.Button((KeyName(keys[i]) + "  x").c_str(), (label + " key " + std::to_string(i)).c_str(), kChip))
                    removed = i;
            if (capture.on)
            {
                if (form.Button(Word(c, WindowWord::KeyWaiting), (label + " cancel").c_str(), "btn flat chip waiting"))
                    capture.on = false;
                else if (form.keyDown)
                    for (const uint8_t key : HeldKeys(form.keyDown))
                    {
                        if (std::ranges::find(capture.held, key) != capture.held.end()) continue;
                        if (std::ranges::find(keys, key) == keys.end())
                        {
                            keys.push_back(key);
                            form.changed = true;
                        }
                        capture.on = false;
                        break;
                    }
            }
            else if (keys.size() < most && form.Button(Word(c, WindowWord::KeyAdd), (label + " add").c_str(), kQuietChip))
                capture = KeyCapture{true, form.keyDown ? HeldKeys(form.keyDown) : std::vector<uint8_t>{}};
            form.EndLine();
            if (removed)
            {
                keys.erase(keys.begin() + static_cast<std::ptrdiff_t>(*removed));
                form.changed = true;
            }
        }

        // Every setting back to HeadsUp's defaults, after a second press.
        void ResetAll(Context& c, const LayoutSetting& e)
        {
            Form& form = c.form;
            form.Fade(false);
            if (!c.state.confirmReset)
            {
                const std::string label = Labeled(e.name, e.key);
                if (form.Button(label.c_str(), label.c_str(), kResetPill, e.tip)) c.state.confirmReset = true;
                return;
            }
            form.Note(Word(c, WindowWord::ResetAllSettingsNote));
            form.Line();
            if (form.Button(Word(c, WindowWord::ResetAllSettingsResetEverything), "reset everything", kAccentPill))
            {
                c.s                  = Settings{};
                form.changed         = true;
                c.state.confirmReset = false;
            }
            if (form.Button(Word(c, WindowWord::ResetAllSettingsKeepThem), "keep them", kRaisedPill)) c.state.confirmReset = false;
            form.EndLine();
        }

        // The views of the game's windows for one of the help's modes, the one it is in at first: where a view's
        // highest window goes, then each view with its shift (the default unless the player typed another) and the
        // windows it holds.
        void WindowGroups(Context& c)
        {
            Form& form  = c.form;
            Settings& s = c.s;
            form.Fade(!s.moveWindows);
            form.Note(Word(c, WindowWord::GroupsNote));
            const HelpMode mode = c.state.shiftsFor.value_or(WindowHelpMode(s));
            form.Line();
            for (const auto& [shown, word] : {std::pair{HelpMode::On, WindowWord::GroupsHelpOn},
                     std::pair{HelpMode::Off, WindowWord::GroupsHelpOff}})
                if (form.Button(Word(c, word), word == WindowWord::GroupsHelpOn ? "help on" : "help off",
                        shown == mode ? kRaisedPill : kFlatPill, WordTip(c, word)))
                    c.state.shiftsFor = shown;
            form.EndLine();
            const auto m = static_cast<size_t>(mode);
            form.Row(Labeled(Word(c, WindowWord::GroupsStartsAt), "starts-at").c_str(),
                WordTip(c, WindowWord::GroupsStartsAt));
            form.NumberAt("Y", "starts at", s.placedUnder[m], kMaxWindowOffset);
            s.placedUnder[m] = std::max(s.placedUnder[m], 0);
            form.EndRow();
            std::vector<ViewShift>& shifts = s.viewShifts[m];
            for (const WindowView& view : WindowViews())
            {
                const std::string id = std::string(view.id);
                const int fallback   = s.placedUnder[m] - view.top;
                const auto own       = std::ranges::find(shifts, id, &ViewShift::id);
                int dy               = own != shifts.end() ? own->dy : fallback;
                const int was        = dy;
                std::string windows;
                for (const WindowKey& key : view.windows)
                    windows += (windows.empty() ? "" : ", ") + WindowName(ShortName(key));
                form.Row((std::string(view.name) + "##view " + id).c_str(), WordTip(c, WindowWord::GroupsRowTip));
                // Named after the view: a box's own label is the box's id and -name, which "y search" would give
                // the box of the view "search/name".
                const bool reset = form.ResetAt((id + " reset").c_str(), own != shifts.end());
                form.NumberAt("Y", (id + " shift").c_str(), dy, kMaxWindowOffset);
                form.Caption(windows.c_str());
                form.EndRow();
                if (reset || (dy != was && dy == fallback))
                {
                    shifts.erase(own);
                    form.changed = true;
                }
                else if (dy != was)
                {
                    if (own == shifts.end())
                        shifts.push_back(ViewShift{id, dy});
                    else
                        own->dy = dy;
                }
            }
        }

        // Three ways to set every game window at once.
        void WindowDefaults(Context& c)
        {
            Form& form = c.form;
            form.Fade(false);
            form.Line();
            const auto button = [&](WindowWord word, const char* id, const char* look) {
                return form.Button(Word(c, word), id, look, WordTip(c, word));
            };
            if (button(WindowWord::WindowDefaultsHeadsupDefaults, "headsup defaults", kRaisedPill))
            {
                UseHeadsUpWindowDefaults(c.s);
                form.changed = true;
            }
            if (button(WindowWord::WindowDefaultsHeadsupHelp, "headsup help", kRaisedPill))
            {
                UseHelpTextDefaults(c.s);
                form.changed = true;
            }
            if (button(WindowWord::WindowDefaultsGameDefaults, "game defaults", kFlatPill))
            {
                UseGameWindowDefaults(c.s);
                form.changed = true;
            }
            form.EndLine();
        }

        // The game's windows HeadsUp moves out from under the help bar, and those it has seen there.
        void WindowList(Context& c)
        {
            Form& form         = c.form;
            Settings& s        = c.s;
            WindowFilter& list = c.state.windows;
            form.Fade(false);
            form.Note(Word(c, WindowWord::WindowListNote));
            {
                form.Row(Labeled(Word(c, WindowWord::WindowListFind), "find").c_str(), nullptr);
                form.TextAt("find window", list.text, Word(c, WindowWord::WindowListNameHint), kFilterWidth, true, kMaxFindText);
                // Which windows are listed is the window's state, and no setting to save.
                const bool was = form.changed;
                form.CheckAt("all windows", list.all, WordTip(c, WindowWord::WindowListAllWindows));
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
                if (!(list.all || label != nullptr ||
                        std::ranges::any_of(s.windowOffsets, [&](const WindowOffset& o) { return o.name == name; }) ||
                        (c.readings.seenWindows != nullptr && has(*c.readings.seenWindows, name))))
                    continue;
                if (Matches(name, list.text.c_str()) || (label != nullptr && Matches(label, list.text.c_str())))
                    listed.push_back(name);
            }
            for (const std::string& window : listed)
            {
                const char* label      = WindowLabel(window);
                const std::string name = TitleCaseText(label != nullptr ? std::string(label) : window); // the game's own
                form.Row((name + "##" + name + " window " + window).c_str(), WordTip(c, WindowWord::WindowListRowTip));
                auto offset = std::ranges::find(s.windowOffsets, window, &WindowOffset::name);
                int dx = offset != s.windowOffsets.end() ? offset->x : 0, dy = offset != s.windowOffsets.end() ? offset->y : 0;
                const int oldX = dx, oldY = dy;
                form.NumberAt("X", ("x " + window).c_str(), dx, kMaxWindowOffset);
                form.NumberAt("Y", ("y " + window).c_str(), dy, kMaxWindowOffset);
                form.EndRow();
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

        // One widget's options: its switch, its section, what it shows and its own settings, as the layout orders
        // them; a row the widget has nothing for is left out.
        void WidgetOptions(Context& c, const LayoutSetting& e, BarWidget widget)
        {
            Form& form  = c.form;
            Settings& s = c.s;
            const int w = static_cast<int>(widget);
            form.Note(c.layout.OptionsOf(WindowList::BarWidgets)[static_cast<size_t>(w)].tip);
            const bool off = !BarOn(s) || !s.barWidgets[w];
            for (const LayoutRow& row : c.layout.RowsOf(e))
            {
                // The rows every widget has take ids of their own: nothing open or typed is carried to another tab.
                const std::string label = Labeled(row.name, row.key, std::string(" ") + BarWidgetName(widget));
                const std::string own   = Labeled(row.name, row.key);
                form.Fade(off);
                switch (static_cast<WidgetTabsRow>(row.row))
                {
                case WidgetTabsRow::Enabled:
                    form.Fade(!BarOn(s));
                    form.Switch(label.c_str(), s.barWidgets[w], row.tip);
                    break;
                case WidgetTabsRow::Section:
                {
                    auto [section, index] = PlaceOf(s.barSections, widget);
                    int moved             = section;
                    const std::vector<const char*> names = NamesOf(c.layout.OptionsOf(WindowList::BarSections));
                    form.Segments(label.c_str(), moved, names.data(), kBarSectionCount, row.tip);
                    if (moved != section)
                        form.changed |= MoveWidget(s.barSections, static_cast<BarSection>(section), index,
                            static_cast<BarSection>(moved), s.barSections[moved].size());
                    break;
                }
                case WidgetTabsRow::SecondLine:
                    if (HasSecondLine(widget)) form.Switch(label.c_str(), s.barSecondLine[w], row.tip);
                    break;
                case WidgetTabsRow::Icons:
                    if (HasIcons(widget)) form.Switch(label.c_str(), s.barIcons[w], row.tip);
                    break;
                case WidgetTabsRow::Details:
                    if (widget == BarWidget::Target) form.Switch(own.c_str(), s.barTargetDetails, row.tip);
                    break;
                case WidgetTabsRow::Currency:
                    if (widget == BarWidget::Gil)
                        form.NumberDropdown(own.c_str(), s.barCurrency, kGilCurrency, kCurrencyCount - 1,
                            [](int currency) { return std::string(kCurrencyTable[currency].name); }, row.tip);
                    break;
                case WidgetTabsRow::TimeZone:
                    if (widget == BarWidget::EarthTime)
                    {
                        int zone = ClockZoneChoice(s.clockZone);
                        form.NumberDropdown(own.c_str(), zone, 0, kClockZoneChoices - 1,
                            [&](int choice) { return ClockZoneChoiceName(choice, c.layout); }, row.tip);
                        s.clockZone = ClockZoneOfChoice(zone);
                    }
                    break;
                case WidgetTabsRow::ServerTime:
                    if (widget == BarWidget::EarthTime) form.Switch(own.c_str(), s.clockServerTime, row.tip);
                    break;
                case WidgetTabsRow::N24Hour:
                    if (widget == BarWidget::EarthTime) form.Switch(own.c_str(), s.clock24, row.tip);
                    break;
                case WidgetTabsRow::Seconds:
                    if (widget == BarWidget::EarthTime) form.Switch(own.c_str(), s.clockSeconds, row.tip);
                    break;
                case WidgetTabsRow::Layout:
                    if (widget == BarWidget::Job)
                    {
                        const std::vector<const char*> names = NamesOf(c.layout.OptionsOf(WindowList::JobLayout));
                        form.Segments(own.c_str(), s.jobLayout, names.data(), kJobLayoutCount, row.tip, kWideSegmentWidth);
                    }
                    break;
                case WidgetTabsRow::ExpBar:
                    if (widget == BarWidget::Job)
                    {
                        const std::vector<const char*> names = NamesOf(c.layout.OptionsOf(WindowList::ExpBar));
                        form.Segments(own.c_str(), s.expBar, names.data(), kExpBarPlaceCount, row.tip);
                    }
                    break;
                case WidgetTabsRow::JobNames:
                    if (widget == BarWidget::Job) form.Switch(own.c_str(), s.jobNames, row.tip);
                    break;
                case WidgetTabsRow::Latency:
                    if (widget == BarWidget::Connection) form.Switch(own.c_str(), s.barLatency, row.tip);
                    break;
                }
            }
        }

        // A tab for each widget, the picked one's options under the tabs.
        void WidgetTabsOf(Context& c, const LayoutSetting& e)
        {
            Form& form = c.form;
            form.Fade(!BarOn(c.s));
            const std::vector<BarWidget> tabs = WidgetTabs();
            const auto words                  = c.layout.OptionsOf(WindowList::BarWidgets);
            std::vector<const char*> labels;
            for (const BarWidget widget : tabs)
                labels.push_back(words[static_cast<size_t>(widget)].name);
            const auto open = std::ranges::find(tabs, static_cast<BarWidget>(c.state.widget));
            const int count = static_cast<int>(tabs.size());
            const int tab   = form.Tabs("widget", open != tabs.end() ? static_cast<int>(open - tabs.begin()) : 0,
                labels.data(), count, TabsPerRow(count, kWidgetTabRows));
            c.state.widget  = static_cast<int>(tabs[static_cast<size_t>(tab)]);
            WidgetOptions(c, e, tabs[static_cast<size_t>(tab)]);
        }

        // Left, Center and Right as trays sunk into the card, each holding its widgets as raised chips in bar order:
        // a chip drags to another place or tray, and a click picks it, which opens its tab. A lit dot says it is on.
        void Trays(Context& c)
        {
            struct Drop
            {
                BarSection from;
                size_t index;
                BarSection to;
                size_t at;
            };
            Form& form  = c.form;
            Settings& s = c.s;
            int& picked = c.state.widget;
            form.Fade(!BarOn(s));
            std::optional<Drop> drop; // applied after the trays are written, so no list changes under its own loop
            const std::string chipStart = form.Named(kWidgetChip) + "-";
            const auto sections         = c.layout.OptionsOf(WindowList::BarSections);
            const auto widgets          = c.layout.OptionsOf(WindowList::BarWidgets);
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
                form.Raw(std::string("<div class=\"tray\"><div class=\"tray-name\">") +
                         EscapedRml(Said(sections[static_cast<size_t>(section)].name)) + "</div>");
                const std::vector<BarWidget>& held = s.barSections[section];
                for (size_t i = 0; i < held.size(); ++i)
                {
                    const int w          = static_cast<int>(held[i]);
                    const std::string id = form.Id(kWidgetChip + std::to_string(w));
                    form.Raw("<div class=\"btn raised wchip drag\" id=\"" + id + "\" tip=\"" +
                             EscapedRml(Said(widgets[static_cast<size_t>(w)].tip)) + "\"><div class=\"dot\"></div>" +
                             EscapedRml(Said(widgets[static_cast<size_t>(w)].name)) + "</div>");
                    form.Set(id, FormValueKind::Class, kOffClass, {}, form.Off());
                    // A clicked chip's tab is at the page's top, which the trays may have scrolled out of view.
                    if (!form.Off() && form.Clicked(id))
                    {
                        picked           = w;
                        form.scrollToTop = true;
                    }
                    form.Set(id, FormValueKind::Class, "on", {}, s.barWidgets[w]);
                    form.Set(id, FormValueKind::Class, kPickedClass, {}, w == picked);
                    dropped(id, here, i); // dropped on a chip: before it
                }
                // The rest of the tray takes a drop at the section's end.
                const std::string end = form.Id("tray end " + std::to_string(section));
                form.Raw("<div class=\"tray-end\" id=\"" + end + "\"></div></div>");
                dropped(end, here, held.size());
            }
            form.EndLine();
            if (drop) form.changed |= MoveWidget(s.barSections, drop->from, drop->index, drop->to, drop->at);
        }

        // A palette's colors in two columns, the first the longer, each with an arrow back to its default.
        void PaletteSwatches(Context& c, const LayoutSetting& e, Color* colors, const Color* defaults)
        {
            Form& form = c.form;
            form.Fade(!c.s.enabled);
            const auto rows = c.layout.RowsOf(e);
            const int count = static_cast<int>(rows.size());
            const int first = (count + 1) / 2;
            form.Raw("<div class=\"columns\"><div class=\"column\">");
            for (int i = 0; i < count; ++i)
            {
                if (i == first) form.Raw("</div><div class=\"column second\">");
                const LayoutRow& row = rows[static_cast<size_t>(i)];
                form.Swatch(Labeled(row.name, row.key, std::string(" ") + e.key).c_str(), colors[row.row], row.tip,
                    &defaults[row.row]);
            }
            form.Raw("</div></div>");
        }

        // The settings row a visibility row stands for, among its kind's.
        int RowIndexOf(VisibilityRow row)
        {
            switch (row)
            {
            case VisibilityRow::Aggressive: return CategoryIndex(Category::WillAttack);
            case VisibilityRow::Passive: return CategoryIndex(Category::WontAttack);
            case VisibilityRow::NmAggressive: return CategoryIndex(Category::NmWillAttack);
            case VisibilityRow::NmPassive: return CategoryIndex(Category::NmWontAttack);
            case VisibilityRow::NmPlaceholder: return CategoryIndex(Category::Placeholder);
            case VisibilityRow::ClaimedYours: return CategoryIndex(Category::PartyClaim);
            case VisibilityRow::ClaimedOthers: return CategoryIndex(Category::OtherClaim);
            case VisibilityRow::Dead: return CategoryIndex(Category::Dead);
            case VisibilityRow::NoData: return CategoryIndex(Category::Unknown);
            case VisibilityRow::Players: return static_cast<int>(PlayerRow::Player);
            case VisibilityRow::PartyMembers: return static_cast<int>(PlayerRow::PartyMember);
            case VisibilityRow::Friends: return static_cast<int>(PlayerRow::Friend);
            case VisibilityRow::LinkshellMembers: return static_cast<int>(PlayerRow::LinkshellMember);
            case VisibilityRow::Self:
            case VisibilityRow::Npcs: return 0;
            case VisibilityRow::YourPets: return static_cast<int>(PetRow::Yours);
            case VisibilityRow::PartyPets: return static_cast<int>(PetRow::Party);
            case VisibilityRow::FriendPets: return static_cast<int>(PetRow::Friend);
            case VisibilityRow::OthersPets: return static_cast<int>(PetRow::Others);
            }
            return 0;
        }

        // The rows of the kind's page, in the layout's order, each with its settings row.
        template <typename Visit>
        void ForEachVisibilityRow(const Context& c, Visit visit)
        {
            const LayoutSetting* visibility = c.layout.SettingOf(SettingPiece::Visibility);
            if (visibility == nullptr) return;
            for (const LayoutRow& row : c.layout.RowsOf(*visibility))
                if ((row.kinds & KindBit(*c.kind)) != 0) visit(row, RowIndexOf(static_cast<VisibilityRow>(row.row)));
        }

        void Visibility(Context& c)
        {
            Form& form           = c.form;
            Settings& s          = c.s;
            const PlateKind kind = *c.kind;
            KindSettings& k      = s.Kind(kind);
            const bool plates = s.enabled && s.nameplates, outlines = s.enabled && s.outlines;
            const auto modes        = c.layout.OptionsOf(WindowList::PlateMode);
            const auto outlineModes = c.layout.OptionsOf(WindowList::OutlineMode);
            form.Fade(!s.enabled);
            form.Row("##visibility heads", nullptr);
            form.Fade(!plates);
            form.Caption(Labeled(Word(c, WindowWord::VisibilityNameplates), "nameplates").c_str(), "head", kModeWidth);
            form.Fade(!outlines);
            form.Caption(Labeled(Word(c, WindowWord::VisibilityOutlines), "outlines").c_str(), "head", kModeWidth);
            form.EndRow();
            ForEachVisibilityRow(c, [&](const LayoutRow& shown, int r) {
                RowSettings& row = k.rows[r];
                form.Fade(!s.enabled);
                form.Row(Labeled(NameOf(shown, kind), shown.key).c_str(), TipOf(shown, kind));
                form.Fade(!plates);
                int plate = static_cast<int>(row.plate);
                form.DropdownAt((std::string("plate ") + RowKey(kind, r)).c_str(), plate, 0, kPlateModeCount - 1,
                    [&](int m) { return Words(modes[static_cast<size_t>(m)].name); }, kModeWidth, nullptr,
                    [&](int m) { return Words(modes[static_cast<size_t>(m)].tip); });
                row.plate = static_cast<PlateMode>(plate);
                form.Fade(!outlines);
                int outline         = static_cast<int>(row.outline);
                const bool claimRow = kind == PlateKind::Mobs && IsClaimRow(static_cast<Category>(r));
                form.DropdownAt((std::string("outline ") + RowKey(kind, r)).c_str(), outline, 0, OutlineModesFor(kind, r) - 1,
                    [&](int m) { return Words(outlineModes[static_cast<size_t>(m)].name); }, kModeWidth, nullptr, [&](int m) {
                        const LayoutOption& mode = outlineModes[static_cast<size_t>(m)];
                        return Words(claimRow && mode.otherTip != nullptr ? mode.otherTip : mode.tip);
                    });
                row.outline = static_cast<OutlineMode>(outline);
                form.EndRow();
            });
        }

        void OutlineColors(Context& c, const LayoutSetting& e)
        {
            Form& form      = c.form;
            KindSettings& k = c.s.Kind(*c.kind);
            form.Fade(!c.s.enabled || !c.s.outlines);
            const std::vector<ColorSource> sources = SourcesFor(*c.kind, false);
            ForEachVisibilityRow(c, [&](const LayoutRow& shown, int r) {
                RowSettings& row = k.rows[r];
                if (!HasOutlineColor(row.outline)) return;
                form.ColorPickRow(Labeled(NameOf(shown, c.kind), shown.key, " outline").c_str(), row.outlineColor, sources,
                    TipOf(e, c.kind));
            });
        }

        // The three lines' boxes, each named, then the kind's tags as chips to click or drag into them.
        void Design(Context& c)
        {
            Form& form           = c.form;
            const PlateKind kind = *c.kind;
            KindSettings& k      = c.s.Kind(kind);
            DesignEditor& editor = c.state.design;
            const auto lines     = c.layout.OptionsOf(WindowList::PlateLines);
            form.Fade(!c.s.enabled || !c.s.nameplates || !c.plates);
            for (int i = 0; i < kPlateLines; ++i)
            {
                form.Line("line design");
                form.Caption(lines[static_cast<size_t>(i)].name, "line-name");
                if (form.LineBox((std::string("design ") + KindKey(kind) + " " + std::to_string(i)).c_str(), k.lines[i].text,
                        editor.drafts[i]) ||
                    editor.drafts[i].typing)
                    editor.line = i;
                form.EndLine();
            }
            form.Line("line chips");
            for (const Tag tag : TagsFor(kind))
                if (form.TagChip(tag) && PutTag(k.lines[editor.line].text, editor.drafts[editor.line].caret, {tag}))
                    form.changed = true;
            form.EndLine();
        }

        // Every tab shows the same rows; a synced line's tab alone shows only its Sync row and what it uses.
        void LineStyles(Context& c, const LayoutSetting& e)
        {
            Form& form           = c.form;
            const PlateKind kind = *c.kind;
            KindSettings& k      = c.s.Kind(kind);
            DesignEditor& editor = c.state.design;
            const bool off       = !c.s.enabled || !c.s.nameplates || !c.plates;
            const auto lines     = c.layout.OptionsOf(WindowList::PlateLines);
            form.Fade(off);
            const std::vector<const char*> tabs = NamesOf(lines);
            editor.styleTab  = form.Tabs("line style", editor.styleTab, tabs.data(), kPlateLines);
            const int n      = editor.styleTab;
            LineDesign& line = k.lines[n];
            // Each line's controls have ids of their own: one held, open or typed in is not carried to another tab.
            const std::string scope = std::string(" ") + KindKey(kind) + std::to_string(n);
            const std::vector<ColorSource> sources = SourcesFor(kind, true);
            for (const LayoutRow& row : c.layout.RowsOf(e))
            {
                const auto r = static_cast<LineStylesRow>(row.row);
                if (r != LineStylesRow::SyncLineStyle && line.syncWith != kNoSync) continue;
                const std::string label = Labeled(NameOf(row, kind), row.key, scope);
                const char* tip         = TipOf(row, kind);
                form.Fade(off);
                switch (r)
                {
                case LineStylesRow::SyncLineStyle:
                {
                    const std::vector<int> targets = SyncTargets(k, n);
                    const int synced               = SyncEntry(targets, line.syncWith);
                    int sync                       = synced;
                    form.NumberDropdown(label.c_str(), sync, 0, static_cast<int>(targets.size()),
                        [&](int entry) { return SyncEntryLabel(targets, entry, c.layout); }, tip, 0.0f,
                        [&](int entry) { return SyncEntryTip(targets, entry, c.layout); });
                    if (sync != synced) line.syncWith = SyncOfEntry(targets, sync);
                    if (line.syncWith != kNoSync)
                        form.Note(Filled(Said(Word(c, WindowWord::LineStylesUsesStyle)), kLineMark,
                            Said(lines[static_cast<size_t>(line.syncWith)].name)).c_str());
                    break;
                }
                case LineStylesRow::LineVisibility: form.ChecksDropdown(label.c_str(), line.show, ConditionsFor(kind), tip); break;
                case LineStylesRow::Alignment:
                    // The bottom line is what the others are set against.
                    if (n < kPlateLines - 1)
                    {
                        const std::vector<const char*> names = NamesOf(c.layout.OptionsOf(WindowList::Align));
                        form.Segments(label.c_str(), line.align, names.data(), kAlignCount, tip, kControlWidth / kAlignCount);
                    }
                    break;
                case LineStylesRow::Font:
                    form.FontDropdown(label.c_str(), line.font, c.plateFonts, tip, Word(c, WindowWord::DefaultFont));
                    break;
                case LineStylesRow::FontStyle:
                {
                    const std::vector<const char*> names = NamesOf(c.layout.OptionsOf(WindowList::FontStyle));
                    form.Toggles(label.c_str(), line.style, names.data(), kFontStyleCount, tip, kControlWidth / kFontStyleCount);
                    break;
                }
                case LineStylesRow::TextSize:
                    form.SliderPercent(label.c_str(), line.textSize, kMinSizePercent, kMaxSizePercent, tip);
                    break;
                case LineStylesRow::IconSize:
                    form.SliderPercent(label.c_str(), line.iconSize, kMinSizePercent, kMaxSizePercent, tip);
                    break;
                case LineStylesRow::Padding:
                    form.SliderInt(label.c_str(), line.padding, 0, kMaxLinePadding, "%d px", n < kPlateLines - 1 ? tip : row.otherTip);
                    break;
                case LineStylesRow::TextColor: form.ColorPickRow(label.c_str(), line.color, sources, tip); break;
                case LineStylesRow::ShadowStrength:
                    form.SliderPercent(label.c_str(), line.shadow, kMinShadow, kMaxShadow, tip);
                    break;
                case LineStylesRow::ShadowColor:
                    form.Fade(off || line.shadow == 0);
                    form.ColorPickRow(label.c_str(), line.shadowColor, sources, tip);
                    break;
                case LineStylesRow::GlowStrength: form.SliderPercent(label.c_str(), line.glow, kMinGlow, kMaxGlow, tip); break;
                case LineStylesRow::GlowSize:
                    form.Fade(off || line.glow == 0);
                    form.SliderPercent(label.c_str(), line.glowSize, kMinGlowSize, kMaxGlowSize, tip);
                    break;
                case LineStylesRow::GlowColor:
                    form.Fade(off || line.glow == 0);
                    form.ColorPickRow(label.c_str(), line.glowColor, sources, tip);
                    break;
                case LineStylesRow::DisableGlowInCombat:
                    form.Fade(off || line.glow == 0);
                    form.Switch(label.c_str(), line.glowOffInCombat, tip);
                    break;
                }
            }
        }

        void Ornament(Context& c, const LayoutSetting& e)
        {
            Form& form              = c.form;
            const PlateKind kind    = *c.kind;
            OrnamentSettings& o     = c.s.Kind(kind).ornament;
            const bool off          = !c.s.enabled || !c.s.nameplates || !c.plates;
            const std::string scope = std::string(" ") + KindKey(kind) + " ornament";
            for (const LayoutRow& row : c.layout.RowsOf(e))
            {
                const std::string label = Labeled(NameOf(row, kind), row.key, scope);
                const char* tip         = TipOf(row, kind);
                form.Fade(off || o.place == OrnamentPlace::Disabled);
                switch (static_cast<OrnamentRow>(row.row))
                {
                case OrnamentRow::Visibility:
                    form.Fade(off);
                    OptionsDropdown(form, label, o.place, c.layout.OptionsOf(WindowList::OrnamentPlace), tip);
                    break;
                case OrnamentRow::Width:
                    form.SliderInt(label.c_str(), o.width, kMinOrnamentWidth, kMaxOrnamentWidth, "%d px", tip);
                    break;
                case OrnamentRow::Height:
                    form.SliderInt(label.c_str(), o.height, kMinOrnamentThickness, kMaxOrnamentThickness, "%d px", tip);
                    break;
                case OrnamentRow::Color: form.ColorPickRow(label.c_str(), o.color, SourcesFor(kind, false), tip); break;
                }
            }
        }

        // Which of the Detection tag's icons show: aggression and links, then what the mob senses you by.
        void Detection(Context& c, const LayoutSetting& e)
        {
            Form& form  = c.form;
            Settings& s = c.s;
            form.Fade(!s.enabled || !s.nameplates || !Uses(s, {Tag::Detect}));
            const auto rows = c.layout.RowsOf(e);
            for (int d = 0; d < kDetectionIconCount; ++d)
            {
                if (static_cast<DetectionIcon>(d) == DetectionIcon::Sight)
                {
                    form.Caption(Labeled(Word(c, WindowWord::DetectionSenses), "senses").c_str(), "line-name");
                    form.Indent(true);
                }
                const LayoutRow& row = rows[static_cast<size_t>(d)];
                const uint32_t bit   = DetectionBit(static_cast<DetectionIcon>(d));
                bool on              = (s.detectionShown & bit) != 0;
                form.Switch(Labeled(row.name, row.key).c_str(), on, row.tip, Word(c, WindowWord::DetectionHide),
                    Word(c, WindowWord::DetectionShow));
                s.detectionShown = on ? s.detectionShown | bit : s.detectionShown & ~bit;
            }
            form.Indent(false);
        }

        // Each status's switch, then the weakest and the least resisted rank that gets a badge.
        void StatusTable(Context& c, const LayoutSetting& e)
        {
            Form& form           = c.form;
            Settings& s          = c.s;
            const bool dim       = !s.enabled || !s.nameplates || !Uses(s, {Tag::Status});
            const auto cutoff    = [](int rank) { return CutoffText(rank); };
            const auto weakTip   = [](int rank) { return CutoffTip(rank, true); };
            const auto resistTip = [](int rank) { return CutoffTip(rank, false); };
            form.Fade(dim);
            form.Row("##status heads", nullptr);
            form.Caption("", "head", kToggleWidth);
            form.Caption(Labeled(Word(c, WindowWord::StatusTableWeak), "weak").c_str(), "head", kCutoffWidth);
            form.Caption(Labeled(Word(c, WindowWord::StatusTableResist), "resist").c_str(), "head", kCutoffWidth);
            form.EndRow();
            for (const LayoutRow& row : c.layout.RowsOf(e))
            {
                StatusBadgeRule& rule = s.statusBadges[row.row];
                const std::string key = row.key;
                form.Fade(dim);
                form.Row(Labeled(row.name, key).c_str(), row.tip);
                form.Raw("<div class=\"switch-cell\">");
                form.CheckAt(("status " + key).c_str(), rule.show);
                form.Raw("</div>");
                form.Fade(dim || !rule.show);
                form.DropdownAt(("weak " + key).c_str(), rule.weakRank, kMaxWeakRank, kNoWeakRank, cutoff, kCutoffWidth, nullptr,
                    weakTip);
                form.DropdownAt(("resist " + key).c_str(), rule.resistRank, kMinResistRank, kNoResistRank, cutoff,
                    kCutoffWidth, nullptr, resistTip);
                form.EndRow();
            }
        }

        // A list of HeadsUp's own pictures, named by the layout's options, then the cursors folder's files by name.
        template <typename Style>
        PictureChoice<Style> PictureDropdown(Context& c, const LayoutSetting& e, const std::string& label, const char* tip,
            Style style, const std::string& file)
        {
            static const std::vector<std::string> kNoFiles;
            const auto options = c.layout.OptionsOf(e);
            const std::vector<PictureChoice<Style>> choices =
                PictureChoices(c.readings.cursorFiles != nullptr ? *c.readings.cursorFiles : kNoFiles, style, file);
            const auto said = [&](int choice) { return options[static_cast<size_t>(choices[static_cast<size_t>(choice)].style)]; };
            int picked      = PictureChoiceOf(choices, style, file);
            c.form.NumberDropdown(label.c_str(), picked, 0, static_cast<int>(choices.size()) - 1,
                [&](int choice) {
                    const PictureChoice<Style>& shown = choices[static_cast<size_t>(choice)];
                    return shown.style == Style::File ? shown.file : Words(said(choice).name);
                },
                tip, 0.0f, [&](int choice) { return Words(said(choice).tip); });
            if (c.form.Opened(label)) c.state.cursorsRequested = true;
            return choices[static_cast<size_t>(picked)];
        }

        void TargetCursorStyle(Context& c, const LayoutSetting& e, const std::string& label, const char* tip)
        {
            Settings& s = c.s;
            c.form.Fade(!s.enabled);
            const PictureChoice<CursorStyle> choice = PictureDropdown(c, e, label, tip, s.cursorStyle, s.cursorFile);
            // Another style comes at its own width, or it would be squeezed into the one before's.
            if (choice.style != s.cursorStyle && choice.style != CursorStyle::Vanilla)
                s.cursorWidth = std::clamp(NaturalCursorWidth(choice.style, s.cursorHeight), kMinCursorSide, kMaxCursorSide);
            s.cursorStyle = choice.style;
            s.cursorFile  = choice.file;
        }

        // A setting, wherever the layout puts it. Each says when it shows, its card's condition included, as it may
        // be moved out of that card.
        void DrawSetting(Context& c, const LayoutSetting& e)
        {
            Form& form              = c.form;
            Settings& s             = c.s;
            const MenuReadings& r   = c.readings;
            const std::string label = Labeled(NameOf(e, c.kind), e.key);
            const char* tip         = TipOf(e, c.kind);
            const bool on = s.enabled, plates = s.enabled && s.nameplates, outlines = s.enabled && s.outlines;
            const bool cursor = s.enabled && s.cursorStyle != CursorStyle::Vanilla;
            const bool shown  = plates && c.plates; // a kind's design, on a page whose rows draw a nameplate
            static const Settings kDefaults;
            static const Palette kPalette = DefaultPalette();
            const auto cutoff             = [](int rank) { return CutoffText(rank); };
            switch (e.piece)
            {
            // Entities
            case SettingPiece::BaseFontSize:
                form.Fade(!plates);
                form.SliderInt(label.c_str(), s.baseSize, kMinTextSize, kMaxTextSize, "%d px", tip);
                break;
            case SettingPiece::DefaultFont:
                form.Fade(!plates);
                form.FontDropdown(label.c_str(), s.defaultFont, c.plateFonts, tip);
                break;
            case SettingPiece::BaseIconSize:
                form.Fade(!plates);
                form.SliderInt(label.c_str(), s.baseIconSize, kMinTextSize, kMaxTextSize, "%d px", tip);
                break;
            case SettingPiece::AutoCheck:
                form.Fade(!plates);
                form.Switch(label.c_str(), s.autoCheck, tip);
                break;
            case SettingPiece::ReplaceDashes:
            {
                form.Fade(!plates);
                const auto options = c.layout.OptionsOf(e);
                int dash           = static_cast<int>(s.dashMark);
                form.MarkupDropdown(label.c_str(), dash, {Words(options[0].name), kDotMark, kSquareMark, kDiamondMark}, tip,
                    [&](int mark) { return Words(options[static_cast<size_t>(mark)].tip); });
                s.dashMark = static_cast<DashMark>(dash);
                break;
            }
            case SettingPiece::ScaleWithDistance:
                form.Fade(!plates);
                form.Switch(label.c_str(), s.scaleWithDistance, tip);
                break;
            case SettingPiece::MinSize:
            case SettingPiece::MaxSize:
            case SettingPiece::ScaleYourOwnName:
                form.Fade(!plates || !s.scaleWithDistance);
                form.Indent(true);
                if (e.piece == SettingPiece::MinSize)
                    form.SliderPercent(label.c_str(), s.scaleMin, kMinSizePercent, kMaxSizePercent, tip);
                else if (e.piece == SettingPiece::MaxSize)
                    form.SliderPercent(label.c_str(), s.scaleMax, kMinSizePercent, kMaxSizePercent, tip);
                else
                    form.Switch(label.c_str(), s.scaleOwnName, tip);
                form.Indent(false);
                break;
            case SettingPiece::VerticalOffset:
                form.Fade(!plates);
                form.SliderInt(label.c_str(), s.nameRaise, kMinNameRaise, kMaxNameRaise, "%d px", tip);
                break;
            case SettingPiece::StackNames:
                form.Fade(!plates);
                form.Switch(label.c_str(), s.stackNames, tip);
                break;
            case SettingPiece::HideDuringConversation:
                form.Fade(!plates);
                form.Switch(label.c_str(), s.hideInEvent, tip);
                break;
            case SettingPiece::Thickness:
                form.Fade(!outlines);
                form.SliderPercent(label.c_str(), s.outlineThickness, kMinOutlineThickness, kMaxOutlineThickness, tip);
                break;
            case SettingPiece::Opacity:
                form.Fade(!outlines);
                form.SliderPercent(label.c_str(), s.outlineOpacity, kMinOpacity, kMaxOpacity, tip);
                break;
            case SettingPiece::Smoothness:
            {
                form.Fade(!outlines);
                const std::vector<const char*> names = NamesOf(c.layout.OptionsOf(WindowList::Smoothness));
                int smoothness = static_cast<int>(SmoothnessLevelOf(s.smoothness));
                form.SliderNamed(label.c_str(), smoothness, names.data(), kSmoothnessLevelCount, tip);
                s.smoothness = SmoothnessCopies(static_cast<SmoothnessLevel>(smoothness));
                break;
            }
            case SettingPiece::OutlineRange:
                form.Fade(!outlines);
                form.SliderFloat(label.c_str(), s.maxDistance, kMinOutlineDistance, kMaxOutlineDistance, "%.0f yalms", tip);
                break;
            case SettingPiece::OutlineFadeIn:
                form.Fade(!outlines);
                form.Switch(label.c_str(), s.outlineFadeIn, Near(tip).c_str());
                break;
            case SettingPiece::StencilNote:
                form.Fade(!outlines);
                if (!r.stencilAvailable) form.Note(NameOf(e, c.kind));
                break;
            case SettingPiece::CursorStyle: TargetCursorStyle(c, e, label, tip); break;
            case SettingPiece::Width:
                form.Fade(!cursor);
                form.SliderInt(label.c_str(), s.cursorWidth, kMinCursorSide, kMaxCursorSide, "%d px", tip);
                break;
            case SettingPiece::CursorHeight:
                form.Fade(!cursor);
                form.SliderInt(label.c_str(), s.cursorHeight, kMinCursorSide, kMaxCursorSide, "%d px", tip);
                break;
            case SettingPiece::OutlineThickness:
                form.Fade(!cursor);
                form.SliderPercent(label.c_str(), s.cursorOutlineThickness, kMinCursorEdge, kMaxCursorEdge, tip);
                break;
            case SettingPiece::Scale:
                form.Fade(!cursor);
                form.Switch(label.c_str(), s.scaleCursor, tip);
                break;
            case SettingPiece::Float:
                form.Fade(!cursor);
                form.Switch(label.c_str(), s.cursorFloat, tip);
                break;
            case SettingPiece::CursorVerticalOffset:
                form.Fade(!cursor);
                form.SliderInt(label.c_str(), s.cursorRaise, -kMaxCursorRaise, kMaxCursorRaise, "%d px", tip);
                break;
            case SettingPiece::Color:
                form.Fade(!cursor || CursorIsPicture(s.cursorStyle));
                form.ColorPickRow(label.c_str(), s.cursorColor, CursorSources(), tip);
                break;
            case SettingPiece::EdgeColor:
                form.Fade(!cursor);
                form.ColorPickRow(label.c_str(), s.cursorEdgeColor, CursorSources(), tip);
                break;
            case SettingPiece::AggressionColors: PaletteSwatches(c, e, s.palette.aggression, kPalette.aggression); break;
            case SettingPiece::ToughnessColors: PaletteSwatches(c, e, s.palette.toughness, kPalette.toughness); break;
            case SettingPiece::ClaimColors: PaletteSwatches(c, e, s.palette.claim, kPalette.claim); break;
            case SettingPiece::TargetingColors: PaletteSwatches(c, e, s.palette.target, kPalette.target); break;
            case SettingPiece::EntityTypeColors: PaletteSwatches(c, e, s.palette.type, kPalette.type); break;
            // The entity pages
            case SettingPiece::Visibility: Visibility(c); break;
            case SettingPiece::OutlineColors: OutlineColors(c, e); break;
            case SettingPiece::Design: Design(c); break;
            case SettingPiece::MaxIconsShown:
                form.Fade(!shown);
                if (HasStatusIcons(*c.kind))
                    form.NumberDropdown(label.c_str(), s.Kind(*c.kind).maxIcons, 1, kCountedIcons,
                        [&](int most) {
                            return most == kCountedIcons ? Words(Word(c, WindowWord::MaxIconsShownAll)) : std::to_string(most);
                        },
                        tip);
                break;
            case SettingPiece::UnusedTags:
                form.Fade(!shown);
                // Under the chips, so nothing above moves under the mouse when it comes and goes.
                if (const std::string note = TagsNote(c, s.Kind(*c.kind), {Tag::Name}, WindowWord::UnusedTagsName); !note.empty())
                    form.Note(note.c_str());
                break;
            case SettingPiece::LineStyles: LineStyles(c, e); break;
            case SettingPiece::Ornament: Ornament(c, e); break;
            case SettingPiece::NoTarget:
                form.Fade(!shown);
                form.SliderPercent(label.c_str(), s.Kind(*c.kind).opacity.noTarget, kMinOpacity, kMaxOpacity, tip);
                break;
            case SettingPiece::NotTargeted:
                form.Fade(!shown);
                form.SliderPercent(label.c_str(), s.Kind(*c.kind).opacity.notTargeted, kMinOpacity, kMaxOpacity, tip);
                break;
            case SettingPiece::IsTargeted:
                form.Fade(!shown);
                form.SliderPercent(label.c_str(), s.Kind(*c.kind).opacity.targeted, kMinOpacity, kMaxOpacity, tip);
                break;
            case SettingPiece::FadeIn:
                form.Fade(!shown);
                form.Switch(label.c_str(), s.Kind(*c.kind).opacity.fadeIn, Near(tip).c_str());
                break;
            case SettingPiece::Range:
                form.Fade(!shown || !s.Kind(*c.kind).opacity.fadeIn);
                form.SliderFloat(label.c_str(), s.Kind(*c.kind).opacity.range, kMinOutlineDistance, kMaxOutlineDistance,
                    "%.0f yalms", tip);
                break;
            case SettingPiece::Detection: Detection(c, e); break;
            // The Target widget's popup reads the ranks switch and these colors too, whatever the nameplates do or hold.
            case SettingPiece::BadgeFont:
                form.Fade(!on);
                form.FontDropdown(label.c_str(), s.badgeFontName, c.plateFonts, tip, Word(c, WindowWord::DefaultFont));
                break;
            case SettingPiece::BadgeNumbers:
            {
                form.Fade(!on);
                const std::vector<const char*> names = NamesOf(c.layout.OptionsOf(e));
                form.Segments(label.c_str(), s.rankDisplay, names.data(), kRankDisplayCount, tip, kWideSegmentWidth);
                break;
            }
            case SettingPiece::CountYourAccuracy:
                form.Fade(!on || s.rankDisplay != RankDisplay::IncreasedChance);
                form.Switch(label.c_str(), s.countAccuracy, tip);
                break;
            case SettingPiece::WeakColor:
                form.Fade(!on);
                form.Swatch(label.c_str(), s.weakBadgeColor, tip, &kDefaults.weakBadgeColor);
                break;
            case SettingPiece::ResistColor:
                form.Fade(!on);
                form.Swatch(label.c_str(), s.resistBadgeColor, tip, &kDefaults.resistBadgeColor);
                break;
            case SettingPiece::StatusResistColor:
                form.Fade(!on);
                form.Swatch(label.c_str(), s.rankBadgeColor, tip, &kDefaults.rankBadgeColor);
                break;
            case SettingPiece::ImmuneColor:
                form.Fade(!on);
                form.Swatch(label.c_str(), s.immuneBadgeColor, tip, &kDefaults.immuneBadgeColor);
                break;
            case SettingPiece::WeakFrom:
                form.Fade(!plates || !Uses(s, {Tag::Weak, Tag::Resist}));
                form.NumberDropdown(label.c_str(), s.elementWeakRank, kMaxWeakRank, kNoWeakRank, cutoff, tip, 0.0f,
                    [](int rank) { return CutoffTip(rank, true); });
                break;
            case SettingPiece::ResistFrom:
                form.Fade(!plates || !Uses(s, {Tag::Weak, Tag::Resist}));
                form.NumberDropdown(label.c_str(), s.elementResistRank, kMinResistRank, kNoResistRank, cutoff, tip, 0.0f,
                    [](int rank) { return CutoffTip(rank, false); });
                break;
            case SettingPiece::MaxElements:
                form.Fade(!plates || !Uses(s, {Tag::Weak, Tag::Resist}));
                form.NumberDropdown(label.c_str(), s.maxElements, kElementCount, 0, ElementCountText, tip, 0.0f, ElementCountTip);
                break;
            case SettingPiece::StatusTable: StatusTable(c, e); break;
            case SettingPiece::RespawnTimers:
                form.Fade(!on);
                form.Switch(label.c_str(), s.phTimers, tip);
                break;
            case SettingPiece::TimerSize:
                form.Fade(!on || !s.phTimers);
                form.SliderPercent(label.c_str(), s.timerPercent, kMinSizePercent, kMaxSizePercent, tip);
                break;
            // User Interface
            case SettingPiece::Key:
            {
                form.Fade(false);
                std::vector<uint8_t> key;
                if (s.screenshotKey != 0) key.push_back(static_cast<uint8_t>(s.screenshotKey));
                Keys(c, label, tip, key, 1, c.state.shotKeyCapture);
                s.screenshotKey = key.empty() ? 0 : key.front();
                break;
            }
            case SettingPiece::HideGameWindows:
                form.Fade(false);
                form.Switch(label.c_str(), s.screenshotHidesGame, tip);
                break;
            case SettingPiece::Groups: WindowGroups(c); break;
            case SettingPiece::MoveWindows:
                form.Fade(false);
                form.Switch(label.c_str(), s.moveWindows, tip);
                break;
            case SettingPiece::KeepClear:
                form.Fade(!s.moveWindows);
                form.Switch(label.c_str(), s.keepClear, tip);
                break;
            case SettingPiece::AutoAdjust:
                form.Fade(!s.moveWindows || r.windowsBand == 0);
                if (form.Action(label.c_str(), tip))
                {
                    AutoAdjustWindows(s, r.windowsBand, r.barBand);
                    form.changed = true;
                }
                break;
            case SettingPiece::GameHelp:
                form.Fade(false);
                form.Switch(label.c_str(), s.moveGameHelp, tip);
                break;
            case SettingPiece::WindowDefaults: WindowDefaults(c); break;
            case SettingPiece::WindowList: WindowList(c); break;
            // Top Panel and Widgets
            case SettingPiece::Theme:
                form.Fade(!BarOn(s));
                OptionsDropdown(form, label, s.barTheme, c.layout.OptionsOf(WindowList::BarTheme), tip);
                break;
            case SettingPiece::Separators:
            {
                form.Fade(!BarOn(s));
                const std::vector<const char*> names = NamesOf(c.layout.OptionsOf(e));
                form.Segments(label.c_str(), s.barSeparators, names.data(), kBarSeparatorsCount, tip, kWideSegmentWidth);
                break;
            }
            case SettingPiece::TitleFont:
                form.Fade(!BarOn(s));
                form.FontDropdown(label.c_str(), s.barTitleFont, c.fonts, tip, Word(c, WindowWord::ThemesFont));
                break;
            case SettingPiece::TextFont:
                form.Fade(!BarOn(s));
                form.FontDropdown(label.c_str(), s.barTextFont, c.fonts, tip, Word(c, WindowWord::ThemesFont));
                break;
            case SettingPiece::Height:
                form.Fade(!BarOn(s));
                form.SliderInt(label.c_str(), s.barHeight, kMinBarHeight, kMaxBarHeight, "%d px", tip);
                break;
            case SettingPiece::TextSize:
            {
                form.Fade(!BarOn(s));
                std::string textSize = "%d px";
                if (s.barTextSize == kAutoBarTextSize)
                    textSize = "Auto";
                else if (r.barTitlePixels > 0 && r.barTitlePixels != s.barTextSize)
                    textSize = Formatted("%%d px (%d fits)", r.barTitlePixels);
                form.SliderInt(label.c_str(), s.barTextSize, kAutoBarTextSize, kMaxBarTextSize, textSize.c_str(), tip);
                break;
            }
            case SettingPiece::OneLine:
            {
                form.Fade(!BarOn(s));
                const std::vector<const char*> names = NamesOf(c.layout.OptionsOf(e));
                form.Segments(label.c_str(), s.barSubtitle, names.data(), kSubtitlePlaceCount, tip);
                break;
            }
            case SettingPiece::PopupSize:
                form.Fade(!BarOn(s));
                form.SliderInt(label.c_str(), s.popupTextSize, kAutoPopupTextSize, kMaxBarTextSize,
                    s.popupTextSize == kAutoPopupTextSize ? "Auto" : "%d px", tip);
                break;
            case SettingPiece::HideReadout:
                form.Fade(!on);
                form.Switch(label.c_str(), s.hideNetstat, tip);
                break;
            case SettingPiece::HideGameHelp:
                form.Fade(!on);
                form.Switch(label.c_str(), s.hideHelpBar, tip);
                break;
            case SettingPiece::Position:
                form.Fade(!BarOn(s) || !s.barWidgets[kHelpWidget]);
                form.Offset(label.c_str(), s.helpBarX, s.helpBarY, kMaxHelpBarOffset, tip);
                break;
            case SettingPiece::Background:
                form.Fade(!BarOn(s) || !s.barWidgets[kHelpWidget]);
                form.Switch(label.c_str(), s.helpBarBackground, tip);
                break;
            case SettingPiece::WidgetTabs: WidgetTabsOf(c, e); break;
            case SettingPiece::Trays: Trays(c); break;
            case SettingPiece::ResetLayout:
                form.Fade(!BarOn(s));
                if (form.Action(label.c_str(), tip) && s.barSections != DefaultPlacement())
                {
                    s.barSections = DefaultPlacement();
                    form.changed  = true;
                }
                break;
            // Mouse Cursor and HeadsUp Config
            case SettingPiece::PointerIcon:
            {
                form.Fade(!on);
                const PictureChoice<PointerIcon> choice = PictureDropdown(c, e, label, tip, s.pointerIcon, s.pointerFile);
                s.pointerIcon = choice.style;
                s.pointerFile = choice.file;
                break;
            }
            case SettingPiece::PointerFix:
                form.Fade(!on);
                form.Switch(label.c_str(), s.fixPointer, tip);
                break;
            case SettingPiece::KeepPointer:
                form.Fade(!on);
                Keys(c, label, tip, s.keepPointerKeys, kMaxKeptKeys, c.state.keyCapture);
                break;
            case SettingPiece::WindowTheme:
                form.Fade(false);
                OptionsDropdown(form, label, s.menuTheme, c.layout.OptionsOf(WindowList::MenuTheme), tip);
                break;
            case SettingPiece::Font:
                form.Fade(false);
                form.FontDropdown(label.c_str(), s.menuFont, c.menuFonts, tip);
                break;
            case SettingPiece::Size:
                form.Fade(false);
                form.Stepper(label.c_str(), s.menuFontSize, kMinMenuFontSize, kMaxMenuFontSize, "%d px", tip);
                break;
            case SettingPiece::WindowOpacity:
                form.Fade(false);
                form.SliderPercent(label.c_str(), s.menuOpacity, kMinMenuOpacity, kMaxOpacity, tip);
                break;
            case SettingPiece::ResetAllSettings: ResetAll(c, e); break;
            case SettingPiece::EnableHeadsup:
                form.Fade(false);
                form.Switch(label.c_str(), s.enabled, tip);
                break;
            case SettingPiece::DebugReport:
                form.Fade(!s.debugMode);
                form.Line(s.debugMode ? "line" : (std::string("line ") + kOffClass).c_str());
                if (form.Button(label.c_str(), label.c_str(), kWashPill, tip)) c.state.debugRequested = true;
                if (r.watching.has_value() &&
                    form.Button(Labeled(Word(c, *r.watching ? WindowWord::DebugReportStop : WindowWord::DebugReportWatch), "watch").c_str(),
                        "watch", kWashPill, WordTip(c, WindowWord::DebugReportWatch)))
                    c.state.watchRequested = true;
                form.EndLine();
                if (r.lastReport != nullptr && !r.lastReport->empty())
                    form.Note(Filled(Word(c, WindowWord::DebugReportSaved), kFileMark, FileName(*r.lastReport)).c_str());
                break;
            case SettingPiece::MobsOutlined:
                form.Fade(!s.debugMode);
                form.Reading(label.c_str(), std::to_string(r.outlinedMobs));
                break;
            case SettingPiece::MeshesOutlined:
                form.Fade(!s.debugMode);
                form.Reading(label.c_str(), std::to_string(r.meshes));
                break;
            case SettingPiece::StencilBuffer:
                form.Fade(!s.debugMode);
                form.Reading(label.c_str(), r.stencilAvailable ? "available" : "missing: no outlines");
                break;
            case SettingPiece::Nameplates:
                form.Fade(!s.debugMode);
                form.Reading(label.c_str(), std::to_string(r.nameplates));
                break;
            case SettingPiece::Drawn:
                form.Fade(!s.debugMode);
                form.Reading(label.c_str(), r.drewInScene ? "into the scene" : "on top");
                break;
            case SettingPiece::GameSNames:
                form.Fade(!s.debugMode);
                form.Reading(label.c_str(),
                    r.hooked ? Formatted("%u, %u by HeadsUp, %u hidden", r.names, r.replaced, r.hidden) : "not hooked");
                break;
            case SettingPiece::FrameTime:
                form.Fade(!s.debugMode);
                form.Reading(label.c_str(), Formatted("%.2f ms (%.0f fps)", r.frameMs, r.frameMs > 0.0 ? 1000.0 / r.frameMs : 0.0));
                break;
            case SettingPiece::LastSave:
                form.Fade(!s.debugMode);
                form.Reading(label.c_str(),
                    Formatted("%.0f ms, %zu %s", r.lastSaveMs, r.lastSaveKeys, r.lastSaveKeys == 1 ? "key" : "keys"));
                break;
            case SettingPiece::YourLevel:
                form.Fade(!s.debugMode);
                form.Reading(label.c_str(),
                    r.level > 0 ? Formatted("%d%s", r.level, r.sitting ? ", sitting" : "") : std::string("unknown"));
                break;
            }
        }

        // What a card does of its own, wherever it is: whether it is left out, whether it is switched off, and the
        // switch in its title.
        struct CardState
        {
            bool skipped  = false;
            bool off      = false;
            bool* enabled = nullptr;
            bool bare     = false; // drawn under the cards with no card of its own
        };
        CardState CardStateOf(CardPiece piece, Context& c)
        {
            Settings& s       = c.s;
            const bool on     = s.enabled;
            const bool plates = s.enabled && s.nameplates;
            switch (piece)
            {
            case CardPiece::Plain:
            case CardPiece::ScreenshotMode:
            case CardPiece::WindowGroups:
            case CardPiece::GameWindows:
            case CardPiece::SettingsWindow: return {};
            case CardPiece::Reset: return {.bare = true};
            case CardPiece::Nameplates: return {false, !on, &s.nameplates};
            case CardPiece::OutlineStyle: return {false, !on, &s.outlines};
            case CardPiece::TargetCursor:
            case CardPiece::ColorsAggression:
            case CardPiece::ColorsToughness:
            case CardPiece::ColorsClaimStatus:
            case CardPiece::ColorsTargeting:
            case CardPiece::ColorsEntityType:
            case CardPiece::Visibility:
            case CardPiece::Badges:
            case CardPiece::PlaceholderTimers:
            case CardPiece::MousePointer: return {false, !on};
            case CardPiece::OutlineDesign: return {c.kind && !c.outlineColors, !on || !s.outlines};
            case CardPiece::NameplateDesign:
            case CardPiece::LineStyle:
            case CardPiece::Ornament:
            case CardPiece::NameplateOpacity: return {c.kind && !c.plates, !plates};
            case CardPiece::DetectionIcons:
            case CardPiece::Weaknesses:
            case CardPiece::Statuses: return {false, !plates};
            case CardPiece::TopPanel: return {false, !on, &s.showBar};
            case CardPiece::HelpBar: return {false, !BarOn(s), &s.barWidgets[kHelpWidget]};
            case CardPiece::Widgets:
            case CardPiece::Layout: return {false, !BarOn(s)};
            case CardPiece::DebugMode: return {false, false, &s.debugMode};
            }
            return {};
        }

        // A card whose settings show only through tags says which, while no line of the mobs' design holds one.
        void CardNote(CardPiece piece, Context& c)
        {
            const auto note = [&](std::initializer_list<Tag> tags) {
                if (const std::string text = TagsNote(c, c.s.Kind(PlateKind::Mobs), tags, WindowWord::UnusedTagsThese); !text.empty())
                    c.form.Note(text.c_str());
            };
            switch (piece)
            {
            case CardPiece::DetectionIcons: note({Tag::Detect}); break;
            case CardPiece::Badges: note({Tag::Weak, Tag::Resist, Tag::Status, Tag::Immune}); break;
            case CardPiece::Weaknesses: note({Tag::Weak, Tag::Resist}); break;
            case CardPiece::Statuses: note({Tag::Status}); break;
            default: break;
            }
        }
    }

    void BuildMenu(Form& form, Settings& s, const MenuReadings& readings, MenuFormState& state,
        const std::vector<std::string>& fonts, const std::vector<std::string>& menuFonts, const WindowLayout& layout)
    {
        form.layout = &layout;
        Context c{form, s, readings, state, fonts, menuFonts, layout, KindOfPage(state.page)};
        Sidebar(c);
        const LayoutPage& page = layout.PageOf(state.page);
        form.pageKey           = page.key;
        if (c.kind)
        {
            if (state.design.kind != *c.kind) state.design = DesignEditor{.kind = *c.kind};
            const KindSettings& k = s.Kind(*c.kind);
            for (int r = 0; r < RowCountOf(*c.kind); ++r)
            {
                c.outlineColors |= HasOutlineColor(k.rows[r].outline);
                c.plates |= DrawsPlate(k.rows[r].plate);
            }
        }
        const uint8_t kinds = c.kind ? KindBit(*c.kind) : kEveryKind;
        for (const LayoutCard& card : layout.CardsOf(page))
        {
            if ((card.kinds & kinds) == 0) continue;
            const CardState own = CardStateOf(card.piece, c);
            if (own.skipped) continue;
            form.Fade(own.off);
            if (own.bare)
                form.BeginBare(Labeled(card.name, card.key).c_str());
            else if (!form.BeginCard(Labeled(card.name, card.key).c_str(), own.enabled, card.about))
                continue;
            CardNote(card.piece, c);
            for (const LayoutSetting& setting : layout.SettingsOf(card))
                if ((setting.kinds & kinds) != 0 && (!setting.entity || c.kind)) DrawSetting(c, setting);
            form.EndCard();
        }
        form.Fade(false);
        // A page whose every card is switched off says what switches them on.
        if (form.CardsShown() == 0 && form.BeginCard(Labeled(Word(c, WindowWord::SwitchedOff), "switched-off").c_str()))
        {
            form.Note(Word(c, s.enabled ? WindowWord::SwitchedOffTopPanel : WindowWord::SwitchedOffHeadsup));
            form.EndCard();
        }
        s = Clamp(s);
    }
}
