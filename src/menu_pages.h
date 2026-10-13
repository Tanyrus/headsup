#pragma once

#include "menu.h"
#include "menu_ui.h"

#include <string>
#include <unordered_set>
#include <vector>

namespace headsup
{
    // The pages of menu_entities.cpp: what every kind of entity shares, and one kind's rows and design.
    void DrawEntitiesPage(Ui& ui, Settings& s, const MenuStatus& status, std::unordered_set<std::string>& collapsed,
        const std::vector<std::string>& fonts);
    void DrawKindPage(Ui& ui, Settings& s, PlateKind kind, std::unordered_set<std::string>& collapsed,
        const std::vector<std::string>& fonts, DesignEditor& editor);

    // A row of keys in menu.cpp: each a chip that a click removes, and while there are fewer than most, a chip that
    // takes the next key pressed that was not already held.
    void DrawKeys(Ui& ui, const char* label, const char* tip, std::vector<uint8_t>& keys, size_t most, KeyCapture& capture);

    // The pages of menu_bar.cpp: the windows HeadsUp moves and screenshot mode, the top panel with its help bar, and
    // the panel's widgets.
    void DrawUserInterfacePage(Ui& ui, Settings& s, const MenuStatus& status, std::unordered_set<std::string>& collapsed,
        WindowList& windows, KeyCapture& shotKey);
    void DrawTopPanelPage(Ui& ui, Settings& s, const MenuStatus& status, std::unordered_set<std::string>& collapsed,
        const std::vector<std::string>& fonts);
    // picked: the widget whose tab is open, by BarWidget.
    void DrawWidgetsPage(Ui& ui, Settings& s, std::unordered_set<std::string>& collapsed, int& picked);
}
