#include "menu.h"

#include "Ashita.h"
#include "bar_theme.h"
#include "pointer_keys.h"

#include <cctype>
#include <cfloat>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <initializer_list>
#include <iterator>
#include <optional>
#include <string_view>
#include <unordered_map>
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
        constexpr ImVec4 kKnob        = Hex(0xFFF4F0);
        constexpr ImVec4 kKnobOff     = Hex(0xB59A97);
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

        // Every size in ems, the font size the menu draws its text at, so it holds at any UI scale.
        constexpr float kPercent        = 100.0f;
        constexpr float kTitleScale     = 1.55f; // the window's name
        constexpr float kPageTitleScale = 1.4f;
        constexpr float kCardTitleScale = 1.12f;
        constexpr float kWindowWidth    = 46.0f;
        constexpr float kWindowHeight   = 36.0f;
        constexpr float kMinWindowWidth  = 34.0f;
        constexpr float kMinWindowHeight = 16.0f;
        constexpr float kSidebarWidth   = 8.5f;
        constexpr float kGutter         = 1.0f;  // around the window's parts and between cards
        constexpr float kCardPad        = 0.9f;  // a card's content from its edges
        constexpr float kCardRadius     = 0.6f;
        constexpr float kRadius         = 0.4f;  // controls
        constexpr float kSwitchWidth    = 2.3f;
        constexpr float kSwitchHeight   = 1.25f;
        constexpr float kKnobInset      = 0.18f;
        constexpr float kSegmentWidth   = 4.6f;
        constexpr float kTrayInset      = 0.22f; // a segment or chip inside its tray
        constexpr float kControlWidth   = 12.0f; // sliders and dropdowns
        constexpr float kCutoffWidth    = 5.2f;  // a status badge's cutoff dropdowns
        constexpr float kSideSegmentWidth = 3.6f; // a player icon's side
        constexpr float kNumberWidth    = 4.2f;  // a number to type
        constexpr float kNumberName     = 1.1f;  // the X or Y before it
        constexpr float kDragSpeed      = 0.5f;  // a dragged number's change for each pixel
        constexpr float kFilterWidth    = 12.0f;
        constexpr float kGroupNameWidth = 9.0f;
        constexpr float kLetterWidth    = 0.55f; // a letter of the menu's fonts, at most on average
        constexpr float kWideSegmentWidth = 5.4f; // a choice as long as "Diamonds"
        constexpr float kChipHeight     = 1.9f;
        constexpr float kChipDot        = 0.5f;
        constexpr float kTipWidth       = 20.0f;
        constexpr float kOffAlpha       = 0.42f; // the settings of a feature that is off are drawn this faint
        constexpr float kHeaderRow      = 2.4f;  // a card's title row

        enum Page : int
        {
            kGlobalPage,
            kOutlinesPage,
            kMobsPage,
            kPlayersPage,
            kNpcsPage,
            kCursorPage,
            kBarPage,
            kWidgetsPage,
            kDebugPage,
            kPageCount,
        };
        const char* const kPageLabels[kPageCount] = {"Global", "Outlines", "Mobs", "Players", "NPCs", "Cursor",
            "Top Bar", "Widgets", "Debug"};

        // One bit each in Menu::m_Collapsed.
        enum Section : uint64_t
        {
            kTextSection          = 1ull << 0,
            kScaleSection         = 1ull << 1,
            kTextColorsSection    = 1ull << 2,
            kOutlineSection       = 1ull << 3,
            kMobsSection          = 1ull << 4,
            kMobColorsSection     = 1ull << 5,
            kMobDisplaySection    = 1ull << 6,
            kMobSizesSection      = 1ull << 7,
            kHideSection          = 1ull << 8,
            kPlaceholdersSection  = 1ull << 9,
            kConColorsSection     = 1ull << 10,
            kAccentColorsSection  = 1ull << 11,
            kPlayerDisplaySection = 1ull << 12,
            kPlayerSizesSection   = 1ull << 13,
            kPlayerIconsSection   = 1ull << 14,
            kNpcDisplaySection    = 1ull << 15,
            kCursorSection        = 1ull << 16,
            kPointerSection       = 1ull << 17,
            kCursorColorsSection  = 1ull << 18,
            kDebugGeneralSection  = 1ull << 19,
            kDebugStatsSection    = 1ull << 20,
            kBadgesSection        = 1ull << 23,
            kStatusBadgesSection  = 1ull << 24,
            kBarSection           = 1ull << 25,
            kBarWidgetsSection    = 1ull << 26,
            kBarWindowsSection    = 1ull << 27,
            kHelpBarSection       = 1ull << 28,
            kWidgetSection        = 1ull << 29,
            kMenuSection          = 1ull << 30,
            kWindowGroupsSection  = 1ull << 31,
            kResetSection         = 1ull << 32,
            kMobIconsSection      = 1ull << 33,
            kDetectionSection     = 1ull << 34,
            kImmunitiesSection    = 1ull << 35,
        };

        const char* const kCategoryLabels[kCategoryCount] = {"Aggressive", "Passive", "No data", "Aggressive NM",
            "Passive NM", "NM placeholder", "Your claim", "Others' claim"};
        const char* const kPlayerIconLabels[kPlayerIconCount] = {"GM", "Mentor", "New adventurer", "Level sync", "Away",
            "Linkshell", "Bazaar", "Seeking party"}; // GM's is never listed: it always shows
        const char* const kIconSideLabels[kIconSideCount] = {"Left", "Right", "Hide"};
        constexpr int kGroupSideCount                      = static_cast<int>(IconSide::Right) + 1; // a group cannot hide
        const char* const kStatusLabels[kStatusRankCount] = {"Paralyze", "Bind", "Silence", "Slow", "Poison", "Light sleep",
            "Dark sleep", "Blind", "Stun", "Gravity"};
        const char* const kMobIdLabels[kMobIdFormatCount]  = {"Off", "Hex", "Decimal", "Both"};
        const char* const kPlayerIconTips[kPlayerIconCount] = {nullptr, nullptr, nullptr,
            "Level synced, outside battlefields.", nullptr, "In the linkshell's color.", nullptr, nullptr};
        const char* const kCategoryTips[kCategoryCount] = {
            "Would attack you at your level. Too Weak mobs only attack while you sit or rest.",
            "Passive, or aggressive but Too Weak for you.",
            "Not in the Phoenix data.",
            "Notorious monsters that would attack you.",
            "Notorious monsters that would leave you alone.",
            "Lottery placeholders, whose death can pop an NM. Off, they take any mob's color.",
            "Claimed by you or your party. Off, they keep the color they had.",
            "Claimed by anyone outside your party. Off, they keep the color they had."};
        // Ordinary mobs, notorious monsters and their placeholders, claimed mobs, then mobs with no data.
        const Category kCategoryOrder[kCategoryCount] = {Category::WillAttack, Category::WontAttack, Category::NmWillAttack,
            Category::NmWontAttack, Category::Placeholder, Category::PartyClaim, Category::OtherClaim, Category::Unknown};
        const char* const kShadeLabels[kLabelShadeCount] = {"Unknown level", "Too Weak", "Easy Prey", "Decent Challenge",
            "Even Match", "Tough", "Very Tough"};
        const char* const kBarWidgetLabels[] = {"Zone", "Weather", "Vana'diel", "Target", "Gil",
            "Bags", "Clock", "Game help", "Server time", "FPS", "Job", "Connection", "Party", "Conquest",
            "Signet", "Distance"};
        const char* const kBarWidgetTips[] = {
            "The zone, its region and who holds it.",
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
        constexpr const char* kWidgetPayload = "HU_BAR_WIDGET";

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

        // A cutoff as its dropdown shows it: the rank as badges read it, or Off past the end.
        std::string CutoffText(int rank, bool ranks)
        {
            return rank == kNoWeakRank || rank == kNoResistRank ? "Off" : RankText(rank, ranks);
        }

        std::string ElementCountText(int count)
        {
            if (count == 0) return "None";
            return count == kElementCount ? "All" : std::to_string(count);
        }

        // A label as the menu shows it: every word capitalized, after a space or a hyphen ("Sub-Target", "24-Hour").
        std::string TitleCase(const char* begin, const char* end)
        {
            std::string text(begin, end);
            bool start = true;
            for (char& c : text)
            {
                if (start) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
                start = c == ' ' || c == '-';
            }
            return text;
        }
        std::string TitleCase(const char* text) { return TitleCase(text, text + std::strlen(text)); }

        // Bundled fonts are always there; Windows' are where its Fonts folder has their file.
        bool MenuFontInstalled(const MenuFont& font)
        {
            return font.file == nullptr || GetFileAttributesA(WindowsFontPath(font.file).c_str()) != INVALID_FILE_ATTRIBUTES;
        }

        // Whether a mob's icon row shows anything: its detection icons or any part of its badge group.
        bool AnyBadges(const Settings& s) { return s.showDamageIcons || s.showStatusRankIcons || s.showImmunityIcons; }
        bool AnyMobIcons(const Settings& s) { return s.showIcons || AnyBadges(s); }

        // Which widgets have a second line, or icons, for their options to offer switching off.
        bool HasSecondLine(BarWidget w)
        {
            return w != BarWidget::EarthTime && w != BarWidget::Fps && w != BarWidget::Connection &&
                   w != BarWidget::Distance && w != BarWidget::GameHelp;
        }
        bool HasIcons(BarWidget w) { return w == BarWidget::Weather || w == BarWidget::Target || w == BarWidget::Gil; }

        void ApplyPhoenixStyle(ImGuiStyle& s, float em)
        {
            ImVec4* c                        = s.Colors;
            c[ImGuiCol_Text]                 = kText;
            c[ImGuiCol_TextDisabled]         = kMuted;
            c[ImGuiCol_TextSelectedBg]       = kAccentSoft;
            c[ImGuiCol_InputTextCursor]      = kText;
            c[ImGuiCol_WindowBg]             = kBackground;
            c[ImGuiCol_ChildBg]              = kClear;
            c[ImGuiCol_PopupBg]              = kCard;
            c[ImGuiCol_Border]               = kBorder;
            c[ImGuiCol_FrameBg]              = kControl;
            c[ImGuiCol_FrameBgHovered]       = kHover;
            c[ImGuiCol_FrameBgActive]        = kHover;
            c[ImGuiCol_CheckMark]            = kAccent;
            c[ImGuiCol_SliderGrab]           = kAccent;
            c[ImGuiCol_SliderGrabActive]     = kAccentHover;
            c[ImGuiCol_Button]               = kControl;
            c[ImGuiCol_ButtonHovered]        = kHover;
            c[ImGuiCol_ButtonActive]         = kAccentSoft;
            c[ImGuiCol_Header]               = kAccentSoft;
            c[ImGuiCol_HeaderHovered]        = kAccentSoft;
            c[ImGuiCol_HeaderActive]         = kAccent;
            c[ImGuiCol_Separator]            = kBorder;
            c[ImGuiCol_ScrollbarBg]          = kClear;
            c[ImGuiCol_ScrollbarGrab]        = kControl;
            c[ImGuiCol_ScrollbarGrabHovered] = kHover;
            c[ImGuiCol_ScrollbarGrabActive]  = kAccent;
            c[ImGuiCol_TableBorderLight]     = kClear;
            c[ImGuiCol_TableBorderStrong]    = kClear;
            c[ImGuiCol_DragDropTarget]       = kAccent;
            s.WindowRounding    = kCardRadius * em * 1.5f;
            s.ChildRounding     = kCardRadius * em;
            s.FrameRounding     = kRadius * em;
            s.PopupRounding     = kRadius * em;
            s.GrabRounding      = em;
            s.ScrollbarRounding = em;
            s.TabRounding       = kRadius * em;
            s.WindowBorderSize  = 1.0f;
            s.FrameBorderSize   = 0.0f;
            s.PopupBorderSize   = 1.0f;
            s.WindowPadding     = ImVec2(kGutter * em, kGutter * em);
            s.FramePadding      = ImVec2(0.6f * em, 0.3f * em);
            s.ItemSpacing       = ImVec2(0.5f * em, 0.45f * em);
            s.ItemInnerSpacing  = ImVec2(0.4f * em, 0.3f * em);
            s.CellPadding       = ImVec2(0.0f, 0.0f);
            s.GrabMinSize       = 0.9f * em;
            s.ScrollbarSize     = 0.7f * em;
            s.ButtonTextAlign   = ImVec2(0.5f, 0.5f);
            s.DisabledAlpha     = 1.0f; // decoration drawn disabled keeps its color
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

            // A page's title, with the settings and colors tabs at its right when the page has colors.
            void PageTitle(const char* title, bool* colorsTab)
            {
                const float x = gui->GetCursorPosX(), y = gui->GetCursorPosY();
                Title(kHeading, title, kPageTitleScale);
                if (colorsTab != nullptr)
                {
                    const float width = 2.0f * Em(kSegmentWidth) + 2.0f * Em(kTrayInset);
                    gui->SetCursorPos(ImVec2(x + cardWidth - width, y));
                    int tab = *colorsTab ? 1 : 0;
                    const char* const tabs[] = {"Settings", "Colors"};
                    const bool was = changed;
                    SegmentsAt("##tabs", tab, tabs, 2, Em(kSegmentWidth));
                    changed = was;
                    *colorsTab = tab == 1;
                }
                EndRowAt(x, y, gui->GetFrameHeight() + Em(0.4f));
            }

            // A rounded card, its title row toggling it open; an enabled switch sits at the title's right. Its ground
            // takes the height it had last frame.
            bool BeginCard(const char* title, uint64_t bit, uint64_t& collapsed, bool* enabled = nullptr,
                const char* tip = nullptr)
            {
                cardX   = gui->GetCursorPosX();
                cardTop = gui->GetCursorPosY();
                cardId  = title;
                const bool open    = (collapsed & bit) == 0;
                const float header = Em(kHeaderRow);
                const auto known   = cardHeights.find(cardId);
                const float height = open && known != cardHeights.end() ? known->second : header;
                char id[64];
                std::snprintf(id, sizeof(id), "##card %s", title);
                Ground(id, ImVec2(cardWidth, height), kStill, Em(kCardRadius), kBorder);
                const float arrowX = cardX + Em(0.5f);
                const float arrow  = gui->GetFrameHeight();
                // The switch at the title row's right keeps its own clicks.
                gui->SetCursorPos(ImVec2(arrowX + arrow, cardTop));
                std::snprintf(id, sizeof(id), "##fold %s", title);
                const float switchRoom = enabled != nullptr ? Em(kSwitchWidth) + Em(kCardPad) : 0.0f;
                const bool rowPressed =
                    gui->InvisibleButton(id, ImVec2(cardWidth - switchRoom - (arrowX + arrow - cardX), header));
                const bool hovered = gui->IsItemHovered();
                Tip(tip);
                gui->SetCursorPos(ImVec2(arrowX, cardTop + (header - arrow) / 2.0f));
                std::snprintf(id, sizeof(id), "##arrow %s", title);
                const bool arrowPressed = WithText(hovered ? kText : kMuted, [&] {
                    return WithColors(ButtonColors{kClear, kClear, kClear},
                        [&] { return gui->ArrowButton(id, open ? ImGuiDir_Down : ImGuiDir_Right); });
                });
                if (arrowPressed || rowPressed) collapsed ^= bit;
                gui->SetCursorPos(ImVec2(arrowX + arrow, cardTop + (header - Em(kCardTitleScale)) / 2.0f));
                Title(kHeading, title, kCardTitleScale);
                if (enabled != nullptr)
                {
                    gui->SetCursorPos(ImVec2(cardX + cardWidth - Em(kCardPad) - Em(kSwitchWidth),
                        cardTop + (header - gui->GetFrameHeight()) / 2.0f));
                    std::snprintf(id, sizeof(id), "##enabled %s", title);
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

            // A pill switch: its knob right and the track red when on.
            void SwitchAt(const char* id, bool& on)
            {
                const float x = gui->GetCursorPosX(), y = gui->GetCursorPosY();
                const float frame = gui->GetFrameHeight();
                const float w = Em(kSwitchWidth), h = std::min(Em(kSwitchHeight), frame);
                const float top = y + (frame - h) / 2.0f;
                gui->SetCursorPos(ImVec2(x, top));
                gui->SetNextItemAllowOverlap();
                bool pressed = Shape(id, ImVec2(w, h), on ? kAccentButton : kRaised, h / 2.0f, on ? kClear : kRaisedEdge);
                const float knob = h - 2.0f * Em(kKnobInset);
                gui->SetCursorPos(ImVec2(on ? x + w - Em(kKnobInset) - knob : x + Em(kKnobInset), top + Em(kKnobInset)));
                char knobId[80];
                std::snprintf(knobId, sizeof(knobId), "%s knob", id);
                const ImVec4 color = on ? kKnob : kKnobOff;
                pressed |= Shape(knobId, ImVec2(knob, knob), ButtonColors{color, color, color}, knob / 2.0f);
                if (pressed)
                {
                    on      = !on;
                    changed = true;
                }
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

            // Options as pills in a tray, the picked one filled: XIUI's choice buttons, rounded.
            template <typename Enum>
            void SegmentsAt(const char* id, Enum& value, const char* const* options, int count, float segment)
            {
                const float x = gui->GetCursorPosX(), y = gui->GetCursorPosY();
                const float frame = gui->GetFrameHeight(), inset = Em(kTrayInset);
                const float width = static_cast<float>(count) * segment + 2.0f * inset;
                char tray[80];
                std::snprintf(tray, sizeof(tray), "##tray %s", id);
                Ground(tray, ImVec2(width, frame), kTray, frame / 2.0f, kBorder);
                for (int option = 0; option < count; ++option)
                {
                    gui->SetCursorPos(ImVec2(x + inset + static_cast<float>(option) * segment, y + inset));
                    char label[80];
                    std::snprintf(label, sizeof(label), "%s##%s %d", TitleCase(options[option]).c_str(), id, option);
                    const bool picked = static_cast<int>(value) == option;
                    const float h     = frame - 2.0f * inset;
                    const bool pressed = WithText(picked ? kText : kMuted, [&] {
                        return Shape(label, ImVec2(segment, h), picked ? kAccentButton : kFlat, h / 2.0f);
                    });
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
                const float width = static_cast<float>(count) * segment + 2.0f * Em(kTrayInset);
                const float y     = Row(label, tip, width);
                SegmentsAt(label, value, options, count, segment);
                EndRowAt(cardX, y, gui->GetFrameHeight());
            }

            // Frames as pills, for sliders and dropdowns.
            template <typename Draw>
            auto RoundFrames(Draw draw)
            {
                const float was     = style.FrameRounding;
                style.FrameRounding = gui->GetFrameHeight() / 2.0f;
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
                RoundFrames([&] { return gui->SliderInt(id, &value, lo, hi, format, ImGuiSliderFlags_AlwaysClamp); });
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
                RoundFrames([&] { return gui->SliderFloat(id, &value, lo, hi, format, ImGuiSliderFlags_AlwaysClamp); });
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
                const bool open = RoundFrames([&] { return gui->BeginCombo(id, shown.c_str(), ImGuiComboFlags_HeightLarge); });
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

            void FontDropdown(const char* label, std::string& font, const std::vector<std::string>& choices,
                bool* bold = nullptr)
            {
                const float boldRoom = bold != nullptr ? Em(kSegmentWidth) + style.ItemSpacing.x : 0.0f;
                const float y        = Row(label, nullptr, Em(kControlWidth) + boldRoom);
                const float x        = gui->GetCursorPosX();
                char id[64];
                std::snprintf(id, sizeof(id), "##font %s", label);
                const auto at = std::ranges::find(choices, font);
                int chosen    = 0;
                if (ListPopup(id, font, Em(kControlWidth), static_cast<int>(choices.size()),
                        [&](int i) { return choices[static_cast<size_t>(i)]; },
                        at != choices.end() ? static_cast<int>(at - choices.begin()) : -1, chosen) &&
                    choices[static_cast<size_t>(chosen)] != font)
                {
                    font    = choices[static_cast<size_t>(chosen)];
                    changed = true;
                }
                if (bold != nullptr)
                {
                    gui->SetCursorPos(ImVec2(x + Em(kControlWidth) + style.ItemSpacing.x, y));
                    std::snprintf(id, sizeof(id), "Bold##bold %s", label);
                    const bool on = *bold;
                    if (WithText(on ? kText : kMuted, [&] {
                            return Shape(id, ImVec2(Em(kSegmentWidth), gui->GetFrameHeight()),
                                on ? kAccentButton : kRaised, gui->GetFrameHeight() / 2.0f, on ? kClear : kRaisedEdge);
                        }))
                    {
                        *bold   = !on;
                        changed = true;
                    }
                }
                EndRowAt(cardX, y, gui->GetFrameHeight());
            }

            void SwatchAt(const char* id, Color& color)
            {
                RoundFrames([&] {
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

            // A switch and the color it turns on, side by side.
            void SwitchSwatch(const char* label, bool& on, Color& color, const char* tip)
            {
                const float frame = gui->GetFrameHeight();
                const float y     = Row(label, tip, Em(kSwitchWidth) + style.ItemSpacing.x + frame);
                const float x     = gui->GetCursorPosX();
                char id[64];
                std::snprintf(id, sizeof(id), "##switch %s", label);
                SwitchAt(id, on);
                gui->SetCursorPos(ImVec2(x + Em(kSwitchWidth) + style.ItemSpacing.x, y));
                std::snprintf(id, sizeof(id), "##swatch %s", label);
                Unless(!on, [&] {
                    SwatchAt(id, color);
                    return 0;
                });
                EndRowAt(cardX, y, frame);
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
                RoundFrames([&] {
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

        // The window's own title bar: the name in the bar's title font, the version beside it and a close button.
        void DrawHeader(Ui& ui, double version, bool& open, float width)
        {
            const float x = ui.gui->GetCursorPosX(), y = ui.gui->GetCursorPosY();
            ui.Title(kHeading, "HeadsUp", kTitleScale);
            ui.gui->SameLine();
            char v[32];
            std::snprintf(v, sizeof(v), "v%g", version);
            ui.gui->SetCursorPosY(y + ui.Em(kTitleScale) - ui.gui->GetFontSize());
            ui.Text(kMuted, v);
            const float close = ui.gui->GetFrameHeight();
            ui.gui->SetCursorPos(ImVec2(x + width - close, y));
            if (ui.WithText(kMuted, [&] { return ui.Shape("x##close", ImVec2(close, close), kRaised, close / 2.0f); }))
                open = false;
            ui.EndRowAt(x, y, ui.Em(kTitleScale) + ui.Em(0.5f));
        }

        // The pages as pills, the picked one filled with an accent mark at its left.
        void DrawSidebar(Ui& ui, int& page)
        {
            const float width = ui.Em(kSidebarWidth), height = ui.Em(2.0f);
            const ImVec2 align = ui.style.ButtonTextAlign;
            ui.style.ButtonTextAlign = ImVec2(0.0f, 0.5f);
            const ImVec2 padding     = ui.style.FramePadding;
            ui.style.FramePadding.x  = ui.Em(1.0f);
            for (int p = 0; p < kPageCount; ++p)
            {
                const bool picked = p == page;
                char label[32];
                std::snprintf(label, sizeof(label), "%s##page", kPageLabels[p]);
                const float x = ui.gui->GetCursorPosX(), y = ui.gui->GetCursorPosY();
                ui.gui->SetNextItemAllowOverlap();
                if (ui.WithText(picked ? kText : kLabel,
                        [&] { return ui.Shape(label, ImVec2(width, height), picked ? kPickedPill : kFlat, height / 2.0f); }))
                    page = p;
                if (picked)
                {
                    const float mark = ui.Em(0.35f);
                    ui.gui->SetCursorPos(ImVec2(x + ui.Em(0.4f), y + (height - mark) / 2.0f));
                    ui.Shape("##mark", ImVec2(mark, mark), kAccentButton, mark / 2.0f);
                    ui.EndRowAt(x, y, height);
                }
            }
            ui.style.FramePadding    = padding;
            ui.style.ButtonTextAlign = align;
        }

        void DrawOutlineSettings(Ui& ui, Settings& s, uint64_t& collapsed, const MenuStatus& status)
        {
            ui.Fade(!s.enabled);
            if (ui.BeginCard("Outline", kOutlineSection, collapsed))
            {
                ui.SliderFloat("Thickness", s.thickness, kMinThickness, kMaxThickness, "%.1f px", nullptr);
                ui.SliderInt("Smoothness", s.smoothness, kMinSmoothness, kMaxSmoothness, "%d copies",
                    "More is rounder and costs a little more.");
                ui.SliderFloat("Range", s.maxDistance, kMinOutlineDistance, kMaxOutlineDistance, "%.0f yalms",
                    "Farther mobs get no outline.");
                if (!status.stencilAvailable) ui.Note("Outlines are unavailable: the depth buffer has no stencil bits.");
                ui.EndCard();
            }
            if (ui.BeginCard("Mobs to Outline", kMobsSection, collapsed))
            {
                for (const Category category : kCategoryOrder)
                {
                    const int c = CategoryIndex(category);
                    ui.Switch(kCategoryLabels[c], s.show[c], kCategoryTips[c]);
                }
                ui.EndCard();
            }
            ui.Fade(false);
        }

        void DrawOutlineColors(Ui& ui, Settings& s, uint64_t& collapsed)
        {
            ui.Fade(!s.enabled);
            if (ui.BeginCard("Mob Colors", kMobColorsSection, collapsed))
            {
                for (const Category category : kCategoryOrder)
                {
                    const int c = CategoryIndex(category);
                    ui.Fade(!s.enabled || !s.show[c]);
                    ui.Swatch(kCategoryLabels[c], s.color[c], kCategoryTips[c]);
                }
                ui.EndCard();
            }
            ui.Fade(false);
        }

        // Every setting back to HeadsUp's defaults, after a second press.
        void DrawReset(Ui& ui, Settings& s, uint64_t& collapsed, bool& confirm)
        {
            if (!ui.BeginCard("Reset", kResetSection, collapsed)) return;
            if (!confirm)
            {
                if (ui.Action("Reset All Settings", "Every setting on every page back to its default.")) confirm = true;
            }
            else
            {
                ui.Note("Every setting goes back to its default. This cannot be undone.");
                const float y = ui.gui->GetCursorPosY();
                if (ui.WithText(kText, [&] {
                        ui.gui->SetCursorPosX(ui.cardX + ui.Em(kCardPad));
                        return ui.Shape("Reset Everything", ImVec2(0.0f, 0.0f), kAccentButton,
                            ui.gui->GetFrameHeight() / 2.0f);
                    }))
                {
                    s          = Settings{};
                    ui.changed = true;
                    confirm    = false;
                }
                ui.gui->SameLine();
                if (ui.Shape("Keep Them", ImVec2(0.0f, 0.0f), kRaised, ui.gui->GetFrameHeight() / 2.0f, kRaisedEdge))
                    confirm = false;
                ui.EndRowAt(ui.cardX, y, ui.gui->GetFrameHeight());
            }
            ui.EndCard();
        }

        void DrawGlobalSettings(Ui& ui, Settings& s, uint64_t& collapsed)
        {
            const bool anyNames = ReplacesAnyName(s);
            ui.Fade(!anyNames);
            if (ui.BeginCard("Names", kTextSection, collapsed, nullptr, "Each kind of name has its font on its own page."))
            {
                ui.SliderInt("Raise names", s.nameRaise, kMinNameRaise, kMaxNameRaise, "%d px",
                    "How far above the game's own spot names sit.");
                ui.Switch("Stack names", s.stackNames,
                    "Names that would cover each other rise one over another. Your target's keeps its place.");
                ui.EndCard();
            }
            ui.Fade(false);
            if (ui.BeginCard("Settings Window", kMenuSection, collapsed))
            {
                std::vector<std::string> names;
                int font = 0;
                for (const MenuFont& f : kMenuFonts)
                    if (MenuFontInstalled(f))
                    {
                        if (s.menuFont == f.name) font = static_cast<int>(names.size());
                        names.emplace_back(f.name);
                    }
                const int was = font;
                ui.NumberDropdown("Font", font, 0, static_cast<int>(names.size()) - 1,
                    [&](int i) { return names[static_cast<size_t>(i)]; }, "Titles stay in the bar's font.");
                if (font != was) s.menuFont = names[static_cast<size_t>(font)];
                ui.SliderInt("Size", s.menuScale, kMinMenuScale, kMaxMenuScale, "%d%%", "Drag its corner to resize the window.");
                ui.EndCard();
            }
            ui.Fade(!s.enabled);
            if (ui.BeginCard("Scale with Distance", kScaleSection, collapsed, &s.scaleWithDistance,
                    "Names are larger the nearer their owner is to you. Your name, players and the cursor have their own "
                    "switches."))
            {
                ui.Note("Your name, other players and the cursor each have a switch on their own pages.");
                ui.EndCard();
            }
            ui.Fade(false);
        }

        void DrawGlobalColors(Ui& ui, Settings& s, uint64_t& collapsed)
        {
            const bool anyNames = ReplacesAnyName(s);
            ui.Fade(!anyNames);
            if (ui.BeginCard("Text Colors", kTextColorsSection, collapsed))
            {
                ui.SwitchSwatch("Name color", s.ownNameColor, s.nameColor,
                    "Off, names keep the game's color, which shows claims. On, every name takes this color.");
                ui.Swatch("Name shadow", s.textOutline, "Also edges the target cursor.");
                ui.SliderInt("Shadow strength##name", s.nameShadow, kMinShadowStrength, kMaxShadowStrength, "%d%%",
                    nullptr);
                ui.Fade(!s.enabled || !s.showLabels);
                ui.Swatch("Level shadow", s.labelShadowColor, "Also under placeholder timers.");
                ui.SliderInt("Shadow strength##level", s.labelShadow, kMinShadowStrength, kMaxShadowStrength, "%d%%",
                    nullptr);
                ui.EndCard();
            }
            ui.Fade(false);
        }

        void DrawMobSettings(Ui& ui, Settings& s, uint64_t& collapsed, const std::vector<std::string>& fonts)
        {
            ui.Fade(!s.enabled);
            if (ui.BeginCard("Display", kMobDisplaySection, collapsed))
            {
                ui.Switch("Mob names", s.replaceMobNames, "Draws mob names in your font, in the game's color.");
                ui.Fade(!s.enabled || !s.replaceMobNames);
                ui.FontDropdown("Name font", s.fontName, fonts, &s.fontBold);
                ui.Fade(!s.enabled);
                ui.Switch("Level and con", s.showLabels,
                    "The level range and con, as Lv 20-23 EP-DC. After a /check, its exact level until it respawns.");
                ui.Fade(!s.enabled || !s.showLabels);
                ui.FontDropdown("Level font", s.labelFontName, fonts, &s.labelFontBold);
                ui.Switch("Ornament", s.showOrnament, "A line with a diamond between the level line and the name.");
                ui.Fade(!s.enabled || !s.replaceMobNames);
                ui.Switch("Name glow", s.nameGlow, "In the mob's outline color.");
                ui.Fade(!s.enabled || !s.replaceMobNames || !s.nameGlow);
                ui.Switch("Unglow claimed", s.glowOffWhenFighting, "No glow once your party claims the mob.");
                ui.Fade(!s.enabled);
                ui.SliderInt("Others' opacity", s.untargetedOpacity, kMinOpacity, kMaxOpacity, "%d%%",
                    "Other mobs' names and lines while you have a target, so your target's stand out.");
                ui.Segments("Mob ID", s.mobId, kMobIdLabels, kMobIdFormatCount,
                    "On the level line. Hex is the last 3 digits players name placeholders by; Decimal is the whole ID.");
                ui.EndCard();
            }
            ui.Fade(!s.enabled || !AnyMobIcons(s));
            if (ui.BeginCard("Icons", kMobIconsSection, collapsed, nullptr, "Where the detection icons and badges go."))
            {
                const char* const places[kIconPlaceCount] = {"Edges", "Center", "Inline"};
                ui.Segments("Placement", s.mobIconPlace, places, kIconPlaceCount,
                    "Above the level line at its ends, together over its middle, or on the line itself at their sides.");
                ui.Fade(!s.enabled || !AnyBadges(s));
                ui.Segments("Badge side", s.badgeSide, kIconSideLabels, kGroupSideCount,
                    "Where weaknesses, statuses and immunities go. Sharing a side, detection icons sit nearer the middle.");
                ui.FontDropdown("Badge font", s.badgeFontName, fonts);
                ui.Fade(!s.enabled || !(s.showDamageIcons || s.showStatusRankIcons));
                ui.Switch("Ranks", s.showRanks, "Phoenix's ranks, -3 to 11, instead of percents. -3 is +50% and 4 is -50%.");
                ui.EndCard();
            }
            ui.Fade(!s.enabled);
            if (ui.BeginCard("Detection", kDetectionSection, collapsed, &s.showIcons,
                    "Aggro, linking and how it detects you, in MobDB's icons."))
            {
                ui.Fade(!s.enabled || !s.showIcons);
                ui.Segments("Side", s.detectionIconSide, kIconSideLabels, kGroupSideCount,
                    "Which end of the level line they start from.");
                ui.EndCard();
            }
            ui.Fade(!s.enabled);
            if (ui.BeginCard("Weaknesses", kBadgesSection, collapsed, &s.showDamageIcons,
                    "Damage types and elements it is weak to (green) or resists (red)."))
            {
                const auto cutoff = [&s](int rank) { return CutoffText(rank, s.showRanks); };
                ui.Fade(!s.enabled || !s.showDamageIcons);
                ui.NumberDropdown("Weak from", s.elementWeakRank, kMaxWeakRank, kNoWeakRank, cutoff,
                    "The least element weakness that gets a badge.");
                ui.NumberDropdown("Resist from", s.elementResistRank, kMinResistRank, kNoResistRank, cutoff,
                    "The least element resistance that gets a badge.");
                ui.NumberDropdown("Max elements", s.maxElements, kElementCount, 0, ElementCountText,
                    "Past this many, those nearest neutral are left off.");
                ui.EndCard();
            }
            ui.Fade(!s.enabled);
            if (ui.BeginCard("Statuses", kStatusBadgesSection, collapsed, &s.showStatusRankIcons,
                    "Statuses it is weak to (green) or resists (yellow)."))
            {
                const bool off    = !s.enabled || !s.showStatusRankIcons;
                const auto cutoff = [&s](int rank) { return CutoffText(rank, s.showRanks); };
                const float cut   = ui.Em(kCutoffWidth);
                const float width = ui.Em(kSwitchWidth) + 2.0f * (cut + ui.style.ItemSpacing.x);
                ui.Fade(off);
                {
                    const float y = ui.Row("##statusHeads", nullptr, width);
                    const float x = ui.gui->GetCursorPosX() + ui.Em(kSwitchWidth) + ui.style.ItemSpacing.x;
                    ui.gui->SetCursorPos(ImVec2(x, y));
                    ui.gui->AlignTextToFramePadding();
                    ui.Text(kMuted, "Weak");
                    ui.gui->SetCursorPos(ImVec2(x + cut + ui.style.ItemSpacing.x, y));
                    ui.gui->AlignTextToFramePadding();
                    ui.Text(kMuted, "Resist");
                    ui.EndRowAt(ui.cardX, y, ui.gui->GetFrameHeight());
                }
                for (int i = 0; i < kStatusRankCount; ++i)
                {
                    StatusBadgeRule& rule = s.statusBadges[i];
                    ui.Fade(off);
                    const float y = ui.Row(kStatusLabels[i], nullptr, width);
                    const float x = ui.gui->GetCursorPosX();
                    char id[48];
                    std::snprintf(id, sizeof(id), "##status %s", kStatusLabels[i]);
                    ui.SwitchAt(id, rule.show);
                    ui.Fade(off || !rule.show);
                    ui.gui->SetCursorPos(ImVec2(x + ui.Em(kSwitchWidth) + ui.style.ItemSpacing.x, y));
                    std::snprintf(id, sizeof(id), "weak %s", kStatusLabels[i]);
                    ui.DropdownAt(id, rule.weakRank, kMaxWeakRank, kNoWeakRank, cutoff, cut);
                    ui.gui->SetCursorPos(ImVec2(x + ui.Em(kSwitchWidth) + 2.0f * ui.style.ItemSpacing.x + cut, y));
                    std::snprintf(id, sizeof(id), "resist %s", kStatusLabels[i]);
                    ui.DropdownAt(id, rule.resistRank, kMinResistRank, kNoResistRank, cutoff, cut);
                    ui.EndRowAt(ui.cardX, y, ui.gui->GetFrameHeight());
                }
                ui.EndCard();
            }
            ui.Fade(!s.enabled);
            if (ui.BeginCard("Immunities", kImmunitiesSection, collapsed, &s.showImmunityIcons,
                    "Statuses it cannot take, marked with a red \xC3\x97."))
            {
                ui.Fade(!s.enabled || !s.showImmunityIcons);
                ui.Note("Each immunity's badge is a red \xC3\x97. Its color is on the Colors tab.");
                ui.EndCard();
            }
            ui.Fade(!s.enabled);
            if (ui.BeginCard("Sizes", kMobSizesSection, collapsed))
            {
                ui.Fade(!s.enabled || !s.replaceMobNames);
                ui.SliderInt("Name", s.mobNameSize, kMinTextSize, kMaxTextSize, "%d px", nullptr);
                ui.Fade(!s.enabled || !s.showLabels);
                ui.SliderInt("Level", s.labelSize, kMinTextSize, kMaxTextSize, "%d px", nullptr);
                ui.Fade(!s.enabled || !AnyMobIcons(s));
                ui.SliderInt("Icons", s.iconSize, kMinTextSize, kMaxTextSize, "%d px", nullptr);
                ui.Fade(!s.enabled || !s.showLabels || !s.showOrnament);
                ui.SliderInt("Ornament width", s.ornamentWidth, kMinOrnamentWidth, kMaxOrnamentWidth, "%d px", nullptr);
                ui.SliderInt("Ornament height", s.ornamentThickness, kMinOrnamentThickness, kMaxOrnamentThickness, "%d px",
                    nullptr);
                ui.Fade(!s.enabled || !s.replaceMobNames || !s.nameGlow);
                ui.SliderInt("Glow", s.glowStrength, kMinGlowStrength, kMaxGlowStrength, "%d%%", nullptr);
                ui.SliderInt("Glow size", s.glowSize, kMinGlowSize, kMaxGlowSize, "%d%%", nullptr);
                ui.EndCard();
            }
            ui.Fade(!s.enabled || (!s.showLabels && s.mobId == MobIdFormat::Off && !AnyMobIcons(s)));
            if (ui.BeginCard("Hide Level and Icons", kHideSection, collapsed))
            {
                ui.Switch("Party claimed", s.hideClaimedByParty, "Its name and the cursor stay.");
                ui.Switch("Any claimed", s.hideClaimed, nullptr);
                ui.Switch("Too Weak", s.hideTooWeak, nullptr);
                ui.Switch("While fighting", s.hideWhileEngaged, "Every mob, while you are engaged.");
                ui.EndCard();
            }
            ui.Fade(!s.enabled);
            if (ui.BeginCard("NM Placeholders", kPlaceholdersSection, collapsed))
            {
                ui.Fade(!s.enabled || s.mobId == MobIdFormat::Off);
                ui.Switch("Mark [PH]", s.markPlaceholders, "On the Mob ID of a mob whose death can pop an NM.");
                ui.Fade(!s.enabled);
                ui.Switch("Respawn timers", s.phTimers,
                    "Above your name, a countdown for each placeholder you see die, as Bigmouth Billy [11A] 3:15. It "
                    "reads \"up\" when due.");
                ui.Fade(!s.enabled || !s.phTimers);
                ui.SliderInt("Timer size", s.timerSize, kMinTextSize, kMaxTextSize, "%d px", nullptr);
                ui.EndCard();
            }
            ui.Fade(false);
        }

        void DrawMobColors(Ui& ui, Settings& s, uint64_t& collapsed)
        {
            ui.Fade(!s.enabled || !s.showLabels);
            if (ui.BeginCard("Level and Con", kConColorsSection, collapsed, nullptr,
                    "Easy Prey covers Incredibly Easy Prey, and Very Tough covers Incredibly Tough."))
            {
                for (int k = 0; k < kLabelShadeCount; ++k)
                    ui.Swatch(kShadeLabels[k], s.labelColor[k], nullptr);
                ui.EndCard();
            }
            ui.Fade(!s.enabled);
            if (ui.BeginCard("Glow, Ornament and Icons", kAccentColorsSection, collapsed))
            {
                ui.Fade(!s.enabled || !s.replaceMobNames || !s.nameGlow);
                ui.SwitchSwatch("Glow color", s.ownGlowColor, s.glowColor, "Off, the glow takes the mob's outline color.");
                ui.Fade(!s.enabled || !s.showLabels || !s.showOrnament);
                ui.SwitchSwatch("Ornament color", s.ownOrnamentColor, s.ornamentColor,
                    "Off, the ornament takes the mob's outline color.");
                ui.Fade(!s.enabled || !AnyMobIcons(s));
                ui.Swatch("Icon tint", s.iconTint, "White keeps the icons as they are.");
                ui.Fade(!s.enabled || !(s.showDamageIcons || s.showStatusRankIcons));
                ui.Swatch("Weak", s.weakBadgeColor, nullptr);
                ui.Fade(!s.enabled || !s.showDamageIcons);
                ui.Swatch("Resist", s.resistBadgeColor, nullptr);
                ui.Fade(!s.enabled || !s.showStatusRankIcons);
                ui.Swatch("Status resist", s.rankBadgeColor, nullptr);
                ui.Fade(!s.enabled || !s.showImmunityIcons);
                ui.Swatch("Immune", s.immuneBadgeColor, nullptr);
                ui.EndCard();
            }
            ui.Fade(false);
        }

        // The icons in their order, GM's aside: each row moves up or down, shares the slot above it, and takes a side.
        void DrawPlayerIcons(Ui& ui, Settings& s)
        {
            ui.Note("GM always shows, nearest the name. A shared slot shows the first of its icons a player has.");
            ui.NumberDropdown("Most shown", s.playerIconMax, 1, kMaxPlayerIcons, [](int n) { return std::to_string(n); },
                "Past this many, the icons last in the order are left off.", ui.Em(kCutoffWidth));
            std::vector<size_t> places; // where each icon but GM is in the order
            for (size_t p = 0; p < s.playerIconOrder.size(); ++p)
                if (s.playerIconOrder[p] != PlayerIcon::Gm) places.push_back(p);
            const float frame = ui.gui->GetFrameHeight(), gap = ui.style.ItemSpacing.x;
            const float segment = ui.Em(kSideSegmentWidth), share = ui.Em(kSegmentWidth);
            const float sides   = static_cast<float>(kIconSideCount) * segment + 2.0f * ui.Em(kTrayInset);
            const float width   = 2.0f * frame + share + sides + 3.0f * gap;
            std::optional<std::pair<size_t, size_t>> swap;
            for (size_t k = 0; k < places.size(); ++k)
            {
                const int i   = static_cast<int>(s.playerIconOrder[places[k]]);
                const float y = ui.Row(kPlayerIconLabels[i], kPlayerIconTips[i], width);
                float x       = ui.gui->GetCursorPosX();
                char id[48];
                const auto arrow = [&](const char* name, ImGuiDir way, bool end) {
                    std::snprintf(id, sizeof(id), "##%s %d", name, i);
                    return ui.Unless(end, [&] { return ui.RoundFrames([&] { return ui.gui->ArrowButton(id, way); }); });
                };
                if (arrow("up", ImGuiDir_Up, k == 0)) swap = {places[k - 1], places[k]};
                ui.gui->SetCursorPos(ImVec2(x += frame + gap, y));
                if (arrow("down", ImGuiDir_Down, k + 1 == places.size())) swap = {places[k], places[k + 1]};
                ui.gui->SetCursorPos(ImVec2(x += frame + gap, y));
                // The first icon has no slot above it to share.
                bool& shared = s.playerIconShare[i];
                std::snprintf(id, sizeof(id), "Share##share %d", i);
                const bool on = shared && k > 0;
                if (ui.Unless(k == 0, [&] {
                        return ui.WithText(on ? kText : kMuted, [&] {
                            return ui.Shape(id, ImVec2(share, frame), on ? kAccentButton : kRaised, frame / 2.0f,
                                on ? kClear : kRaisedEdge);
                        });
                    }))
                {
                    shared     = !shared;
                    ui.changed = true;
                }
                ui.Tip("Shares the slot of the icon above it.");
                ui.gui->SetCursorPos(ImVec2(x += share + gap, y));
                std::snprintf(id, sizeof(id), "side %d", i);
                ui.SegmentsAt(id, s.playerIconSide[i], kIconSideLabels, kIconSideCount, segment);
                ui.EndRowAt(ui.cardX, y, frame);
            }
            if (swap)
            {
                std::swap(s.playerIconOrder[swap->first], s.playerIconOrder[swap->second]);
                ui.changed = true;
            }
        }

        void DrawPlayerSettings(Ui& ui, Settings& s, uint64_t& collapsed, const std::vector<std::string>& fonts)
        {
            ui.Fade(!s.enabled);
            if (ui.BeginCard("Display", kPlayerDisplaySection, collapsed))
            {
                ui.Switch("Player names", s.replacePlayerNames, "Draws player names, yours too, in your font.");
                ui.Fade(!s.enabled || !s.replacePlayerNames);
                ui.FontDropdown("Name font", s.playerFontName, fonts, &s.playerFontBold);
                ui.Switch("Icons", s.showPlayerIcons, "Party, bazaar, linkshell and the rest, in HQ. Player Icons places each.");
                ui.Fade(!s.enabled || !s.replacePlayerNames || !s.showPlayerIcons);
                ui.Switch("Center icons", s.centerNameAndIcons, "Centers the name and icons together. Off, the name alone.");
                ui.EndCard();
            }
            ui.Fade(!s.enabled || !s.replacePlayerNames);
            if (ui.BeginCard("Sizes", kPlayerSizesSection, collapsed))
            {
                ui.SliderInt("Your name", s.selfNameSize, kMinTextSize, kMaxTextSize, "%d px", nullptr);
                ui.SliderInt("Players", s.playerNameSize, kMinTextSize, kMaxTextSize, "%d px", nullptr);
                ui.Fade(!s.enabled || !s.replacePlayerNames || !s.showPlayerIcons);
                ui.SliderInt("Icons", s.playerIconSize, kMinPlayerIconSize, kMaxPlayerIconSize, "%d%%", "Of the name's height.");
                ui.Fade(!s.enabled || !s.replacePlayerNames || !s.scaleWithDistance);
                ui.Switch("Scale yours", s.scaleOwnName, "Off, your name stays one size.");
                ui.Switch("Scale players", s.scalePlayerNames, "Off, other players' names stay one size.");
                ui.EndCard();
            }
            ui.Fade(!s.enabled || !s.replacePlayerNames || !s.showPlayerIcons);
            if (ui.BeginCard("Player Icons", kPlayerIconsSection, collapsed))
            {
                DrawPlayerIcons(ui, s);
                ui.EndCard();
            }
            ui.Fade(false);
        }

        void DrawNpcSettings(Ui& ui, Settings& s, uint64_t& collapsed, const std::vector<std::string>& fonts)
        {
            ui.Fade(!s.enabled);
            if (ui.BeginCard("NPC Names", kNpcDisplaySection, collapsed, &s.replaceNpcNames, "Draws NPC names in your font."))
            {
                ui.Fade(!s.enabled || !s.replaceNpcNames);
                ui.FontDropdown("Font", s.npcFontName, fonts, &s.npcFontBold);
                ui.SliderInt("Size", s.npcNameSize, kMinTextSize, kMaxTextSize, "%d px", nullptr);
                ui.EndCard();
            }
            ui.Fade(false);
        }

        bool KeyDown(uint8_t vk) { return (GetAsyncKeyState(vk) & 0x8000) != 0; }

        // The keys that keep the game's pointer, each a chip that a click removes, and a chip that takes the next
        // key pressed that was not already held.
        void DrawKeptKeys(Ui& ui, Settings& s, KeyCapture& capture)
        {
            const float y = ui.Row("Keep pointer", "The game hides its pointer when a key runs a command. While only "
                                                   "these keys are held, it stays.", 0.0f);
            ui.EndRowAt(ui.cardX, y, ui.gui->GetFrameHeight());
            ui.gui->SetCursorPosX(ui.cardX + ui.Em(kCardPad));
            for (size_t i = 0; i < s.keepPointerKeys.size(); ++i)
            {
                char chip[48];
                std::snprintf(chip, sizeof(chip), "%s  x##key%zu", KeyName(s.keepPointerKeys[i]).c_str(), i);
                if (i > 0) ui.gui->SameLine();
                if (ui.Shape(chip, ImVec2(0.0f, 0.0f), kRaised, ui.gui->GetFrameHeight() / 2.0f, kRaisedEdge))
                {
                    s.keepPointerKeys.erase(s.keepPointerKeys.begin() + static_cast<std::ptrdiff_t>(i));
                    ui.changed = true;
                    break;
                }
            }
            if (!s.keepPointerKeys.empty()) ui.gui->SameLine();
            if (!capture.on)
            {
                if (s.keepPointerKeys.size() < kMaxKeptKeys &&
                    ui.Shape("+ Add key", ImVec2(0.0f, 0.0f), kFlat, ui.gui->GetFrameHeight() / 2.0f, kRaisedEdge))
                    capture = KeyCapture{true, HeldKeys(KeyDown)};
                return;
            }
            if (ui.WithText(kMuted, [&] {
                    return ui.Shape("Press a key...##cancel", ImVec2(0.0f, 0.0f), kFlat, ui.gui->GetFrameHeight() / 2.0f,
                        kAccent);
                }))
                capture.on = false;
            for (const uint8_t key : HeldKeys(KeyDown))
            {
                if (std::ranges::find(capture.held, key) != capture.held.end()) continue;
                if (std::ranges::find(s.keepPointerKeys, key) == s.keepPointerKeys.end()) s.keepPointerKeys.push_back(key);
                ui.changed = true;
                capture.on = false;
                break;
            }
        }

        void DrawCursorSettings(Ui& ui, Settings& s, uint64_t& collapsed, KeyCapture& capture)
        {
            ui.Fade(!s.enabled);
            if (ui.BeginCard("Target Cursor", kCursorSection, collapsed, &s.replaceCursor,
                    "Draws HeadsUp's cursor above the nameplate instead of the game's."))
            {
                ui.Fade(!s.enabled || !s.replaceCursor);
                ui.Switch("Feather", s.cursorFeather, "Phoenix's feather instead of the arrow.");
                ui.SliderInt("Size", s.cursorSize, kMinTextSize, kMaxTextSize, "%d px", nullptr);
                ui.SliderInt("Outline", s.cursorOutline, kMinCursorOutline, kMaxCursorOutline, "%d%%",
                    "Its dark edge, as a share of the usual thickness.");
                ui.Switch("Scale", s.scaleCursor, "Larger the nearer your target is.");
                ui.EndCard();
            }
            ui.Fade(!s.enabled);
            if (ui.BeginCard("Mouse Pointer", kPointerSection, collapsed))
            {
                ui.Switch("Chocobo", s.chocoboPointer, "PlayOnline's chocobo as the mouse pointer.");
                ui.Switch("Pointer fix", s.fixPointer,
                    "Keeps the game's pointer under yours after a resize, and clicks where you click. It also shows the "
                    "pointer again over Ashita's windows.");
                DrawKeptKeys(ui, s, capture);
                ui.EndCard();
            }
            ui.Fade(false);
        }

        void DrawCursorColors(Ui& ui, Settings& s, uint64_t& collapsed)
        {
            ui.Fade(!s.enabled || !s.replaceCursor);
            if (ui.BeginCard("Target Cursor", kCursorColorsSection, collapsed))
            {
                ui.Swatch("Target", s.cursorColor, nullptr);
                ui.Swatch("Locked on", s.lockedCursorColor, nullptr);
                ui.Swatch("Sub-target", s.subCursorColor, "Picking for a spell, ability or trade.");
                ui.Swatch("Out of range", s.outOfRangeCursorColor, "A sub-target out of the spell or ability's range.");
                ui.EndCard();
            }
            ui.Fade(false);
        }

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
                ui.RoundFrames([&] {
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
                ui.RoundFrames([&] { return ui.gui->InputText(id, name, sizeof(name), 0, nullptr, nullptr); });
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
                // A chip that would pass the card's edge starts the next line.
                const float rowLeft = left + ui.Em(1.0f);
                const auto place    = [&](bool first, float room) {
                    if (first)
                    {
                        ui.gui->SetCursorPosX(rowLeft);
                        return;
                    }
                    ui.gui->SameLine();
                    if (ui.gui->GetCursorPosX() + room <= right) return;
                    ui.gui->NewLine();
                    ui.gui->SetCursorPosX(rowLeft);
                };
                std::optional<size_t> taken;
                for (size_t w = 0; w < group.windows.size(); ++w)
                {
                    const std::string shown = WindowName(group.windows[w]) + "  x";
                    char chip[64];
                    std::snprintf(chip, sizeof(chip), "%s##member %zu %zu", shown.c_str(), g, w);
                    place(w == 0, ui.ChipRoom(shown.size()));
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
                place(group.windows.empty(), ui.Em(kControlWidth) * 0.8f);
                if (ui.ListPopup(id, "+ Add window", ui.Em(kControlWidth) * 0.8f, static_cast<int>(free.size()),
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
            if (s.windowGroups.size() < kMaxWindowGroups && ui.Action("+ New group"))
            {
                s.windowGroups.push_back(
                    WindowGroup{"Group " + std::to_string(s.windowGroups.size() + 1), 0, 0, false, {}});
                ui.changed = true;
            }
        }

        // A font dropdown whose first choice, kept as an empty name, is the theme's own.
        void ThemeFontDropdown(Ui& ui, const char* label, std::string& font, const std::vector<std::string>& fonts)
        {
            constexpr const char* kThemes = "Theme's";
            std::vector<std::string> choices{kThemes};
            choices.insert(choices.end(), fonts.begin(), fonts.end());
            std::string shown = font.empty() ? kThemes : font;
            ui.FontDropdown(label, shown, choices);
            font = shown == kThemes ? std::string() : shown;
        }

        void DrawBarSettings(Ui& ui, Settings& s, uint64_t& collapsed, const MenuStatus& status, WindowList& windows,
            const std::vector<std::string>& fonts)
        {
            ui.Fade(!s.enabled);
            if (ui.BeginCard("Top Bar", kBarSection, collapsed, &s.showBar,
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
                ThemeFontDropdown(ui, "Title font", s.barTitleFont, fonts);
                ThemeFontDropdown(ui, "Text font", s.barTextFont, fonts);
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
            if (ui.BeginCard("Help Bar", kHelpBarSection, collapsed, &s.barWidgets[help], kBarWidgetTips[help]))
            {
                ui.Fade(!s.enabled);
                ui.Switch("Hide game help", s.hideHelpBar, "The game's own help bar, which the top bar covers.");
                ui.Fade(!BarOn(s) || !s.barWidgets[help]);
                ui.Offset("Position", s.helpBarX, s.helpBarY, kMaxHelpBarOffset,
                    "Pixels from its place under the top bar.");
                ui.Switch("Background", s.helpBarBackground, "Off, the help text alone, with no bar behind it.");
                ui.EndCard();
            }
            // Offsets and groups move windows whether or not the bar shows.
            ui.Fade(false);
            if (ui.BeginCard("Window Groups", kWindowGroupsSection, collapsed))
            {
                DrawWindowGroups(ui, s);
                ui.EndCard();
            }
            if (ui.BeginCard("Game Windows", kBarWindowsSection, collapsed))
            {
                DrawMovedWindows(ui, s, status, windows);
                ui.EndCard();
            }
            ui.Fade(false);
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

        void DrawWidgetsPage(Ui& ui, Settings& s, uint64_t& collapsed, int& picked)
        {
            ui.Fade(!BarOn(s));
            if (ui.BeginCard("Layout", kBarWidgetsSection, collapsed, nullptr,
                    "Drag a chip to move it; click it for its options."))
            {
                std::optional<Drop> drop; // applied after drawing, so no list changes under its own loop
                DrawTrays(ui, s, picked, drop);
                if (drop) ui.changed |= MoveWidget(s.barSections, drop->from, drop->index, drop->to, drop->at);
                if (ui.Action("Reset layout", "Puts every widget back in its first place. Switches stay.") &&
                    s.barSections != DefaultPlacement())
                {
                    s.barSections = DefaultPlacement();
                    ui.changed    = true;
                }
                ui.EndCard();
            }
            if (picked == static_cast<int>(BarWidget::GameHelp) || picked < 0 || picked >= kBarWidgetCount) picked = 0;
            const int w = picked;
            ui.Fade(!BarOn(s));
            if (ui.BeginCard(TitleCase(kBarWidgetLabels[w]).c_str(), kWidgetSection, collapsed, &s.barWidgets[w]))
            {
                DrawWidgetOptions(ui, s, static_cast<BarWidget>(w));
                ui.EndCard();
            }
            ui.Fade(false);
        }

        void DrawDebug(Ui& ui, Settings& s, const MenuStatus& status, uint64_t& collapsed, bool& requested)
        {
            if (ui.BeginCard("HeadsUp", kDebugGeneralSection, collapsed, &s.enabled, "All of HeadsUp, as /hu on and /hu off."))
            {
                if (ui.Action("Debug report", "As /hu debug: writes a report to logs/headsup.")) requested = true;
                ui.EndCard();
            }
            if (ui.BeginCard("This Frame", kDebugStatsSection, collapsed))
            {
                char text[96];
                const auto line = [&](const char* label, const char* value) {
                    const float y = ui.Row(label, nullptr, ui.Em(kControlWidth));
                    ui.gui->AlignTextToFramePadding();
                    ui.Text(kText, value);
                    ui.EndRowAt(ui.cardX, y, ui.gui->GetFrameHeight());
                };
                std::snprintf(text, sizeof(text), "%u", status.outlinedMobs);
                line("Mobs outlined", text);
                std::snprintf(text, sizeof(text), "%u", status.meshes);
                line("Meshes outlined", text);
                line("Stencil buffer", status.stencilAvailable ? "available" : "missing: no outlines");
                std::snprintf(text, sizeof(text), "%u", status.nameplates);
                line("Nameplates", text);
                line("Drawn", status.drewInScene ? "into the scene" : "on top");
                if (status.hooked)
                    std::snprintf(text, sizeof(text), "%u, %u by HeadsUp", status.names.names, status.names.replaced);
                else
                    std::snprintf(text, sizeof(text), "not hooked");
                line("Game's names", text);
                std::snprintf(text, sizeof(text), "%.2f ms (%.0f fps)", status.frameMs,
                    status.frameMs > 0.0 ? 1000.0 / status.frameMs : 0.0);
                line("Frame time", text);
                std::snprintf(text, sizeof(text), "%.0f ms", status.lastSaveMs);
                line("Last save", text);
                if (status.player.level > 0)
                    std::snprintf(text, sizeof(text), "%d%s", status.player.level, status.player.sitting ? ", sitting" : "");
                else
                    std::snprintf(text, sizeof(text), "unknown");
                line("Your level", text);
                ui.EndCard();
            }
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

    namespace
    {
        DWORD WINAPI ListInstalledFonts(LPVOID into)
        {
            *static_cast<std::vector<std::string>*>(into) = InstalledFontFamilies();
            return 0;
        }
    }

    Menu::~Menu() { WaitForFonts(); }

    void Menu::WaitForFonts()
    {
        if (m_FontThread == nullptr) return;
        WaitForSingleObject(m_FontThread, INFINITE);
        CloseHandle(m_FontThread);
        m_FontThread = nullptr;
    }

    void Menu::ListFonts(const std::string& current)
    {
        if (!m_FontsAsked)
        {
            m_FontsAsked = true;
            m_FontThread = CreateThread(nullptr, 0, ListInstalledFonts, &m_Installed, 0, nullptr);
            if (m_FontThread == nullptr) m_Fonts = FontChoices(InstalledFontFamilies(), current);
        }
        if (m_FontThread != nullptr && WaitForSingleObject(m_FontThread, 0) == WAIT_OBJECT_0)
        {
            WaitForFonts();
            m_Fonts = FontChoices(std::move(m_Installed), current);
        }
        // Until Windows' are listed, the bundled fonts and the one in use.
        if (m_Fonts.empty()) m_Fonts = FontChoices({}, current);
    }

    std::string WindowsFontPath(const char* file)
    {
        char windows[MAX_PATH] = {};
        const UINT length      = GetWindowsDirectoryA(windows, MAX_PATH);
        return std::string(windows, length < MAX_PATH ? length : 0) + "\\Fonts\\" + file;
    }

    bool Menu::OpenPage(const std::string& name, bool colors)
    {
        for (int p = 0; p < kPageCount; ++p)
        {
            std::string key;
            for (const char c : std::string(kPageLabels[p]))
                if (c != ' ') key += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (key != name) continue;
            m_Page      = p;
            m_ColorsTab = colors;
            open        = true;
            return true;
        }
        return false;
    }

    void Menu::ScrollTo(float pixels)
    {
        constexpr int kFramesToSettle = 3;
        m_ScrollTo                    = pixels;
        m_ScrollFrames                = kFramesToSettle;
    }

    void Menu::SetFonts(ImFont* text, ImFont* title, float textScale)
    {
        m_TextFont  = text;
        m_TitleFont = title;
        m_TextScale = textScale;
    }

    bool Menu::Draw(IGuiManager* gui, Settings& s, const MenuStatus& status)
    {
        if (!open || gui == nullptr) return false;
        ListFonts(s.fontName);
        // Ashita shares one style between every addon's windows: ours is set for this window and put back after it.
        ImGuiStyle& style       = gui->GetStyle();
        const ImGuiStyle ashita = style;
        const float base        = gui->GetFontSize() * static_cast<float>(s.menuScale) / kPercent;
        const float em          = m_TextFont != nullptr ? base * m_TextScale : base;
        if (m_TextFont != nullptr) gui->PushFont(m_TextFont, em);
        ApplyPhoenixStyle(style, em);
        Ui ui{gui, style, style.Alpha, em, m_TitleFont, m_CardHeights, false, 0.0f, 0.0f, 0.0f, {}, false};

        // Its first size; the player resizes it from its corner, down to what the sidebar and a card need.
        gui->SetNextWindowSize(ImVec2(kWindowWidth * em, kWindowHeight * em), ImGuiCond_FirstUseEver);
        gui->SetNextWindowSizeConstraints(ImVec2(kMinWindowWidth * em, kMinWindowHeight * em), ImVec2(FLT_MAX, FLT_MAX),
            nullptr, nullptr);
        if (gui->Begin("HeadsUp###HeadsUp", &open,
                ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar |
                    ImGuiWindowFlags_NoScrollWithMouse))
        {
            const float inner = gui->GetWindowWidth() - 2.0f * kGutter * em;
            DrawHeader(ui, status.version, open, inner);
            if (gui->BeginTable("##layout", 2, ImGuiTableFlags_SizingFixedFit))
            {
                const float sidebar = kSidebarWidth * em;
                const float content = inner - sidebar - kGutter * em;
                gui->TableSetupColumn("##pages", ImGuiTableColumnFlags_WidthFixed, sidebar + kGutter * em, 0);
                gui->TableSetupColumn("##page", ImGuiTableColumnFlags_WidthFixed, content, 0);
                gui->TableNextRow(0, 0.0f);
                gui->TableNextColumn();
                DrawSidebar(ui, m_Page);
                gui->TableNextColumn();
                // Cards fill the column, less room for the scrollbar.
                ui.cardX     = gui->GetCursorPosX();
                ui.cardWidth = content - style.ScrollbarSize;
                const bool hasColors =
                    m_Page == kGlobalPage || m_Page == kOutlinesPage || m_Page == kMobsPage || m_Page == kCursorPage;
                if (!hasColors) m_ColorsTab = false;
                ui.PageTitle(kPageLabels[m_Page], hasColors ? &m_ColorsTab : nullptr);
                // The title bar, the pages and the page's title stay put: only the cards scroll, in a table that
                // scrolls on its own (ImGui's child windows are overloaded, which abi_check refuses).
                const float rest = gui->GetWindowHeight() - gui->GetCursorPosY() - style.WindowPadding.y;
                if (gui->BeginTable("##page scroll", 1, ImGuiTableFlags_ScrollY, ImVec2(content, rest), 0.0f))
                {
                // A page starts at its top, not where the last one was left.
                if (const int shown = m_Page * 2 + (m_ColorsTab ? 1 : 0); shown != m_ShownPage)
                {
                    if (m_ScrollFrames == 0) ScrollTo(0.0f);
                    m_ShownPage = shown;
                }
                if (m_ScrollFrames > 0)
                {
                    gui->SetScrollY(m_ScrollTo);
                    --m_ScrollFrames;
                }
                gui->TableNextRow(0, 0.0f);
                gui->TableNextColumn();
                ui.cardX     = gui->GetCursorPosX();
                ui.cardWidth = content - style.ScrollbarSize;
                switch (m_Page)
                {
                case kGlobalPage:
                    if (m_ColorsTab)
                        DrawGlobalColors(ui, s, m_Collapsed);
                    else
                    {
                        DrawGlobalSettings(ui, s, m_Collapsed);
                        DrawReset(ui, s, m_Collapsed, m_ConfirmReset);
                    }
                    break;
                case kOutlinesPage:
                    if (m_ColorsTab)
                        DrawOutlineColors(ui, s, m_Collapsed);
                    else
                        DrawOutlineSettings(ui, s, m_Collapsed, status);
                    break;
                case kMobsPage:
                    if (m_ColorsTab)
                        DrawMobColors(ui, s, m_Collapsed);
                    else
                    {
                        DrawMobSettings(ui, s, m_Collapsed, m_Fonts);
                    }
                    break;
                case kPlayersPage:
                    DrawPlayerSettings(ui, s, m_Collapsed, m_Fonts);
                    break;
                case kNpcsPage:
                    DrawNpcSettings(ui, s, m_Collapsed, m_Fonts);
                    break;
                case kCursorPage:
                    if (m_ColorsTab)
                        DrawCursorColors(ui, s, m_Collapsed);
                    else
                        DrawCursorSettings(ui, s, m_Collapsed, m_KeyCapture);
                    break;
                case kBarPage:
                    DrawBarSettings(ui, s, m_Collapsed, status, m_Windows, m_Fonts);
                    break;
                case kWidgetsPage: DrawWidgetsPage(ui, s, m_Collapsed, m_Widget); break;
                default: DrawDebug(ui, s, status, m_Collapsed, m_DebugRequested); break;
                }
                ui.Fade(false);
                gui->EndTable();
                }
                gui->EndTable();
            }
        }
        gui->End();
        if (m_TextFont != nullptr) gui->PopFont();
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
