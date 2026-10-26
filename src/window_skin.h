#pragma once

#include "bar_theme.h"
#include "game_font.h"
#include "menu_document.h"
#include "plate_image.h"
#include "skin_frame.h"

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace headsup
{
    // The game's eight Window Types (Config > Windows > Shared), each a skin of its own: ROM/0/14.DAT for Type 1 to
    // ROM/0/21.DAT for Type 8.
    constexpr int kWindowTypes = 8;
    constexpr int kFollowGameWindow = 0; // a Window Type setting's: the type the game is set to, where 1 to 8 pin one
    std::string WindowSkinFile(int type);

    // A skin's pictures: the background the game tiles every 128 pixels, the strip its top and bottom edges are cut
    // from, the one its left and right edges are, and its four corners; and the background's brightness at the top
    // and at the bottom of a window (1 is the picture as it is), which the game shades it with.
    struct WindowSkin
    {
        ColorPicture back, across, down, corner;
        float shadeTop = 1.0f, shadeBottom = 1.0f;
    };
    // A skin out of its whole DAT.
    Read<WindowSkin> ReadWindowSkin(std::span<const uint8_t> dat);
    // The skins read, by type; none for a type whose file could not be.
    using WindowSkins = std::array<std::optional<WindowSkin>, kWindowTypes>;

    // How the game draws a window, in its menu pixels: the background repeated every tile from the window's corner,
    // edges this thick stretched between corners this large.
    constexpr int kSkinTile = 128, kFrameEdge = 5, kFrameCorner = 24;
    // A size in the game's menu pixels in pixels of the screen, where the menu is drawn scale times as large.
    int SkinPixels(int menuPixels, float scale);

    // The pieces a window is drawn with, as RmlUi loads them, each at a size in pixels: the background square, an edge
    // that thick (and its picture's length, which RmlUi stretches), a corner square, and a clear pixel for the middle
    // of a tiled box, which draws something there.
    enum class SkinPiece : uint8_t
    {
        Back,
        Top,
        Bottom,
        Left,
        Right,
        TopLeft,
        TopRight,
        BottomLeft,
        BottomRight,
        Clear,
    };
    std::string SkinSource(int type, SkinPiece piece, int pixels);
    // The picture a SkinSource name stands for; none for another name, or a skin that was not read.
    std::optional<Image> SkinImage(const WindowSkins& skins, std::string_view name);

    // The colors drawn with a skin: its background's hue, light and vivid, for the accent, text that stands out from
    // the background as shaded, the background and the frame's own colors for the panels and edges.
    BarPalette SkinPalette(const WindowSkin& skin);
    MenuColors SkinMenuColors(const WindowSkin& skin);

    // The skin drawn for a Window Type setting: the type it pins, else the type the game is set to (0 to 7), else Type 1.
    int SkinTypeOf(int windowType, std::optional<int> gameType);

    // What the panel and the settings window are drawn with for one skin: its frame and its colors.
    struct GameSkinLook
    {
        SkinFrame frame;
        BarPalette palette;
        MenuColors menu;
    };
    GameSkinLook SkinLookOf(const WindowSkin& skin, int type, float scale);
}
