#pragma once

#include "window_layout.h"

#include <array>
#include <deque>
#include <string>
#include <string_view>
#include <vector>

namespace headsup
{
    // A layout read from a file while the game runs: its tables and the texts they point into, which stay put.
    struct OwnedLayout
    {
        std::deque<std::string> texts;
        std::deque<std::array<const char*, kPlateKindCount>> byKind;
        std::vector<LayoutPage> pages;
        std::vector<LayoutCard> cards;
        std::vector<LayoutSetting> settings;
        std::vector<LayoutRow> rows;
        std::vector<LayoutOption> options;
        std::vector<LayoutList> lists;
        std::vector<LayoutWord> words;

        WindowLayout View() const { return {pages, cards, settings, rows, options, lists, words}; }
    };

    // Reads a settings_window.json of this version against the built-in layout, which says what every key is and
    // what the code draws, by tools/gen_window.py's rules: the problem, naming the entry that does not fit, or
    // nothing, with the file's layout in out, its tables as the generator would write them.
    std::string ReadWindowFile(std::string_view text, const WindowLayout& builtIn, std::string_view version, OwnedLayout& out);
}
