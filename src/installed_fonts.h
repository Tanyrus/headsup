#pragma once

#include "menu_fonts.h"

#include <string>
#include <vector>

namespace headsup
{
    // A font file in Windows' Fonts folder.
    std::string WindowsFontPath(const char* file);
    // Bundled fonts are always there; Windows' are where its Fonts folder has their file.
    bool MenuFontInstalled(const MenuFont& font);

    // The font families the settings window offers. Windows takes up to a second to list them, so a thread does
    // once they are first asked for: until it is done the choices are the bundled fonts and the one in use.
    class InstalledFonts
    {
    public:
        ~InstalledFonts();
        // Before the plugin unloads: the listing thread runs the plugin's code.
        void Wait();
        // As FontChoices gives them.
        const std::vector<std::string>& Choices(const std::string& current);

    private:
        std::vector<std::string> m_Choices;
        std::vector<std::string> m_Installed; // the thread's, read once it has ended
        void* m_Thread = nullptr;
        bool m_Asked   = false;
    };
}
