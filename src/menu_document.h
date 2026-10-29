#pragma once

#include "skin_frame.h"

#include <cstdint>
#include <optional>
#include <string>

namespace headsup
{
    // The settings window's looks: Phoenix's own, Aurora's glass and switches, and the game's window art. All move
    // alike.
    enum class MenuTheme : uint8_t
    {
        Phoenix,
        Aurora,
        GameWindow,
    };
    constexpr int kMenuThemeCount = static_cast<int>(MenuTheme::GameWindow) + 1;

    // The settings window's colors (ARGB): a page, the cards on it, the controls on those, each a step lighter, and
    // the trays a step below the page.
    struct MenuColors
    {
        uint32_t background, card, control, hover, inset;
        uint32_t border, raisedEdge; // a card's edge, and a chip's, lighter
        uint32_t text, heading, label, muted;
        uint32_t accent, accentHover, accentSoft; // accentSoft: the picked page, a hovered row
        uint32_t accentEnd;                       // what the accent runs into where it is a gradient
        bool operator==(const MenuColors&) const = default;
    };
    // The Phoenix palette of phoenix-xi.com, which the bar's Phoenix themes share: its red runs into its ember.
    constexpr MenuColors kPhoenixMenuColors{0xF7150C0C, 0xFF24181A, 0xFF352325, 0xFF452D2F, 0xFF0F0808, 0x24D2ABAB,
        0x3DD2ABAB, 0xFFFFF4F0, 0xFFE7C9A9, 0xFFD9C2BE, 0xFF8C7270, 0xFFC55151, 0xFFE06F5E, 0x47C55151, 0xFFE06F5E};

    // Aurora: slate glass with a cyan accent that runs into violet, as the bar's theme of that name.
    constexpr MenuColors kAuroraMenuColors{0xEB0B111B, 0xB3162031, 0xFF1F2B40, 0xFF2A3A55, 0xFF080C13, 0x2EB8D8FF,
        0x4DB8D8FF, 0xFFEAF2FF, 0xFFEAF2FF, 0xFFC2D0E6, 0xFF7F92AE, 0xFF5EE6FF, 0xFF9AF0FF, 0x385EE6FF, 0xFFB48CFF};

    struct MenuLook
    {
        MenuTheme theme = MenuTheme::Phoenix;
        std::string textFont, titleFont;
        int fontPixels = 0; // the window's text
        int textLift   = 0; // pixels its capitals stand under the middle of their line, which a line in a box is lifted by
        bool developer = false; // a developer's build: its frame has the layout file's Reload and Export
        MenuColors colors = kPhoenixMenuColors;
        std::optional<SkinFrame> skin; // the game's art the window is drawn in, for its Game Window theme
        bool operator==(const MenuLook&) const = default;
        // The same but for the text's size, the colors and the art: the style changes and the document stays.
        bool SameFrame(const MenuLook& other) const
        {
            return theme == other.theme && textFont == other.textFont && titleFont == other.titleFont && developer == other.developer;
        }
    };
    // RmlUi gives a font's ascent and descent but no cap height: the bundled fonts and Segoe UI stand their capitals
    // 0.7 of their size over the baseline.
    constexpr float kCapHeightShare = 0.7f;
    // How far under the middle of its line a line of capitals stands, from the font's ascent and descent as RmlUi lays
    // the line out: in whole pixels, as RmlUi puts each glyph on a whole pixel.
    int TextLift(float ascent, float descent, int fontPixels);

    // The window's ids, which its owner finds its parts by.
    constexpr const char* kMenuSidebarId   = "sidebar";
    constexpr const char* kMenuPageId      = "page";
    constexpr const char* kMenuPageTitleId = "page-title";
    constexpr const char* kMenuVersionId   = "version";
    constexpr const char* kMenuCloseId     = "close";
    constexpr const char* kMenuReloadId    = "reload";
    constexpr const char* kMenuPopupId     = "popup";
    constexpr const char* kMenuTipId       = "tip";
    constexpr const char* kMenuCardClass   = "card";
    constexpr const char* kMenuFootClass   = "foot"; // a block under the cards with no card around it
    constexpr const char* kSetupPreviewClass = "setup-preview"; // Setup's picture, as wide as its card
    constexpr const char* kMenuTipAttribute = "tip";

    // The settings window's document: its frame, the sidebar and page to fill, the popup and the tip, and its style.
    std::string MenuDocument(const MenuLook& look);
    // The style of every control the kit writes, sized in ems of the window's text.
    std::string MenuStyle(const MenuLook& look);
}
