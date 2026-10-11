#pragma once

#include <string_view>

namespace headsup
{
    // The fonts the settings window can draw its text in: Windows' own, by their file in its Fonts folder, and the
    // ones HeadsUp bundles (file nullptr), each with the size it is drawn at over Ashita's, as condensed faces run
    // small.
    struct MenuFont
    {
        const char* name;
        const char* file;
        float scale;
    };
    inline constexpr MenuFont kMenuFonts[] = {
        {"Segoe UI", "segoeui.ttf", 1.0f},
        {"Segoe UI Bold", "segoeuib.ttf", 1.0f},
        {"Calibri", "calibri.ttf", 1.05f},
        {"Candara", "candara.ttf", 1.05f},
        {"Corbel", "corbel.ttf", 1.05f},
        {"Tahoma", "tahoma.ttf", 0.95f},
        {"Verdana", "verdana.ttf", 0.92f},
        {"Trebuchet MS", "trebuc.ttf", 1.0f},
        {"Georgia", "georgia.ttf", 1.0f},
        {"Constantia", "constan.ttf", 1.05f},
        {"Arial", "arial.ttf", 1.0f},
        {"Barlow Condensed", nullptr, 1.15f},
        {"Marcellus SC", nullptr, 1.05f},
        {"Cinzel", nullptr, 1.0f},
        {"Cormorant SC", nullptr, 1.1f},
    };
    inline constexpr const char* kDefaultMenuFont = "Segoe UI";

    // The font of this name; nullptr for one the list lacks.
    constexpr const MenuFont* MenuFontNamed(std::string_view name)
    {
        for (const MenuFont& font : kMenuFonts)
            if (name == font.name) return &font;
        return nullptr;
    }
}
