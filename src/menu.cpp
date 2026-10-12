#include "menu.h"

#include "Ashita.h"
#include "menu_pages.h"
#include "menu_state.h"
#include "menu_ui.h"
#include "pointer_keys.h"

#include <cfloat>
#include <cstdio>
#include <algorithm>
#include <optional>
#include <utility>

namespace headsup
{
    namespace
    {
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

        // The pages as pills, the picked one filled with an accent mark at its left; a child page is indented.
        void DrawSidebar(Ui& ui, Page& page)
        {
            const float height = ui.Em(2.0f), indent = ui.Em(1.0f);
            const ImVec2 align = ui.style.ButtonTextAlign;
            ui.style.ButtonTextAlign = ImVec2(0.0f, 0.5f);
            const ImVec2 padding     = ui.style.FramePadding;
            ui.style.FramePadding.x  = ui.Em(1.0f);
            for (const PageInfo& info : kPages)
            {
                const float width = ui.Em(kSidebarWidth) - (info.child ? indent : 0.0f);
                const bool picked = info.page == page;
                char label[32];
                std::snprintf(label, sizeof(label), "%s##page", info.label);
                const float x = ui.gui->GetCursorPosX(), y = ui.gui->GetCursorPosY();
                if (info.child) ui.gui->SetCursorPosX(x + indent);
                ui.gui->SetNextItemAllowOverlap();
                if (ui.WithText(picked ? kText : kLabel,
                        [&] { return ui.Shape(label, ImVec2(width, height), picked ? kPickedPill : kFlat, height / 2.0f); }))
                    page = info.page;
                if (picked)
                {
                    const float mark = ui.Em(0.35f);
                    ui.gui->SetCursorPos(ImVec2(x + ui.Em(0.4f) + (info.child ? indent : 0.0f), y + (height - mark) / 2.0f));
                    ui.Shape("##mark", ImVec2(mark, mark), kAccentButton, mark / 2.0f);
                    ui.EndRowAt(x, y, height);
                }
            }
            ui.style.FramePadding    = padding;
            ui.style.ButtonTextAlign = align;
        }

        // Every setting back to HeadsUp's defaults, after a second press.
        void DrawReset(Ui& ui, Settings& s, std::unordered_set<std::string>& collapsed, bool& confirm)
        {
            if (!ui.BeginCard("Reset", collapsed)) return;
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

        void DrawConfigMenuPage(Ui& ui, Settings& s, std::unordered_set<std::string>& collapsed, bool& confirm)
        {
            if (ui.BeginCard("Settings Window", collapsed))
            {
                std::vector<std::string> names;
                for (const MenuFont& f : kMenuFonts)
                    if (MenuFontInstalled(f)) names.emplace_back(f.name);
                ui.FontDropdown("Font", s.menuFont, names, "Titles stay in the bar's font.");
                ui.SliderInt("Size", s.menuScale, kMinMenuScale, kMaxMenuScale, "%d%%", "Drag its corner to resize the window.");
                ui.EndCard();
            }
            DrawReset(ui, s, collapsed, confirm);
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
                    ui.Shape("+ Add Key", ImVec2(0.0f, 0.0f), kFlat, ui.gui->GetFrameHeight() / 2.0f, kRaisedEdge))
                    capture = KeyCapture{true, HeldKeys(KeyDown)};
                return;
            }
            if (ui.WithText(kMuted, [&] {
                    return ui.Shape("Press a Key...##cancel", ImVec2(0.0f, 0.0f), kFlat, ui.gui->GetFrameHeight() / 2.0f,
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

        void DrawMouseCursorPage(Ui& ui, Settings& s, std::unordered_set<std::string>& collapsed, KeyCapture& capture)
        {
            ui.Fade(!s.enabled);
            if (ui.BeginCard("Mouse Pointer", collapsed))
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

        void DrawDebugPage(Ui& ui, Settings& s, const MenuStatus& status, std::unordered_set<std::string>& collapsed, bool& requested)
        {
            if (ui.BeginCard("HeadsUp", collapsed, &s.enabled, "All of HeadsUp, as /hu on and /hu off."))
            {
                if (ui.Action("Debug Report", "As /hu debug: writes a report to logs/headsup.")) requested = true;
                ui.EndCard();
            }
            if (ui.BeginCard("This Frame", collapsed))
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
                    std::snprintf(text, sizeof(text), "%u, %u by HeadsUp, %u hidden", status.names.names,
                        status.names.replaced, status.names.hidden);
                else
                    std::snprintf(text, sizeof(text), "not hooked");
                line("Game's names", text);
                std::snprintf(text, sizeof(text), "%.2f ms (%.0f fps)", status.frameMs,
                    status.frameMs > 0.0 ? 1000.0 / status.frameMs : 0.0);
                line("Frame time", text);
                std::snprintf(text, sizeof(text), "%.0f ms, %zu %s", status.lastSaveMs, status.lastSaveKeys,
                    status.lastSaveKeys == 1 ? "key" : "keys");
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

    bool Menu::OpenPage(const std::string& name)
    {
        const std::optional<Page> page = PageNamed(name);
        if (!page) return false;
        m_Page = *page;
        open   = true;
        return true;
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
        ListFonts(s.defaultFont);
        // Ashita shares one style between every addon's windows: ours is set for this window and put back after it.
        ImGuiStyle& style       = gui->GetStyle();
        const ImGuiStyle ashita = style;
        const float base        = gui->GetFontSize() * static_cast<float>(s.menuScale) / kPercent;
        const float em          = m_TextFont != nullptr ? base * m_TextScale : base;
        if (m_TextFont != nullptr) gui->PushFont(m_TextFont, em);
        ApplyPhoenixStyle(style, em);
        Ui ui{gui, style, style.Alpha, em, m_TitleFont, m_CardHeights, false, 0.0f, 0.0f, 0.0f, {}, {}, false};

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
                ui.pageKey = PageKey(m_Page);
                ui.PageTitle(kPages[static_cast<int>(m_Page)].label);
                // The title bar, the pages and the page's title stay put: only the cards scroll, in a table that
                // scrolls on its own (ImGui's child windows are overloaded, which abi_check refuses).
                const float rest = gui->GetWindowHeight() - gui->GetCursorPosY() - style.WindowPadding.y;
                if (gui->BeginTable("##page scroll", 1, ImGuiTableFlags_ScrollY, ImVec2(content, rest), 0.0f))
                {
                // A page starts at its top, not where the last one was left.
                if (const int shown = static_cast<int>(m_Page); shown != m_ShownPage)
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
                case Page::Entities: DrawEntitiesPage(ui, s, status, m_Collapsed, m_Fonts); break;
                case Page::Mobs:
                case Page::Players:
                case Page::Self:
                case Page::Npcs:
                case Page::Pets: DrawKindPage(ui, s, *KindOfPage(m_Page), m_Collapsed, m_Fonts, m_Design); break;
                case Page::UserInterface: DrawUserInterfacePage(ui, s, status, m_Collapsed, m_Windows); break;
                case Page::TopBar: DrawTopBarPage(ui, s, status, m_Collapsed, m_Fonts, m_Widget); break;
                case Page::MouseCursor: DrawMouseCursorPage(ui, s, m_Collapsed, m_KeyCapture); break;
                case Page::ConfigMenu: DrawConfigMenuPage(ui, s, m_Collapsed, m_ConfirmReset); break;
                case Page::Debug: DrawDebugPage(ui, s, status, m_Collapsed, m_DebugRequested); break;
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
