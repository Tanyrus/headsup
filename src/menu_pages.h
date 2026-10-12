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

    // The pages of menu_bar.cpp: the windows HeadsUp moves, and the top bar with its help bar and widgets.
    void DrawUserInterfacePage(Ui& ui, Settings& s, const MenuStatus& status, std::unordered_set<std::string>& collapsed,
        WindowList& windows);
    void DrawTopBarPage(Ui& ui, Settings& s, const MenuStatus& status, std::unordered_set<std::string>& collapsed,
        const std::vector<std::string>& fonts, int& picked);
}
