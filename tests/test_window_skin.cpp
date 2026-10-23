#include "dat_bytes.h"
#include "game_font.h"
#include "test.h"
#include "window_skin.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

using namespace headsup;

namespace
{
    using namespace dat;

    // A picture of one 565 color, the whole of it.
    Bytes Solid(const char* name, int side, uint16_t color)
    {
        Bytes blocks;
        for (int i = 0; i < side * side / 16; ++i)
            blocks = blocks + ColorBlock(color, color, {});
        return Picture(name, side, side, "DXT1", blocks);
    }

    // The shapes chunk the four pictures are drawn by: its name, the pictures, then its first shape, the background,
    // whose four colors run top left, top right, bottom left, bottom right.
    Bytes Shapes(uint8_t top, uint8_t bottom)
    {
        Bytes out;
        PutText(out, "menu    win00   ");
        out.push_back(4);
        for (const char* name : {"MENU    newtex  ", "MENU    vfr1    ", "MENU    hfr1    ", "MENU    corner  "})
            PutText(out, name);
        Put16(out, 1);
        out.push_back(1);
        for (const int v : {0, 0, 128, 0, 0, 128, 128, 128, 128, 128, 0, 0})
            Put16(out, static_cast<uint16_t>(v));
        out.push_back(0);
        for (const uint8_t level : {top, top, bottom, bottom})
            out.insert(out.end(), {level, level, level, 0x40});
        return out;
    }

    constexpr uint16_t kBlue565 = 0x001F, kRed565 = 0xF800, kGreen565 = 0x07E0, kWhite565 = 0xFFFF;

    Bytes SkinDat(bool withDown = true, uint8_t top = 0x40, uint8_t bottom = 0x7F, int corner = 32)
    {
        Bytes out = Chunk("win0", 0x01, Bytes(16, 0)) + Chunk("newt", 0x20, Solid("menu    newtex  ", 128, kBlue565)) +
                    Chunk("hfr1", 0x20, Solid("menu    hfr1    ", 32, kRed565));
        if (withDown) out = out + Chunk("vfr1", 0x20, Solid("menu    vfr1    ", 32, kGreen565));
        return out + Chunk("corn", 0x20, Solid("menu    corner  ", corner, kWhite565)) + Chunk("win0", 0x31, Shapes(top, bottom));
    }

    // A picture's first pixel as B, G, R, A.
    std::vector<uint8_t> First(const ColorPicture& p) { return {p.bgra.begin(), p.bgra.begin() + 4}; }
}

TEST(a_skin_is_its_four_pictures_and_the_backgrounds_shade_from_its_shapes)
{
    const Read<WindowSkin> read = ReadWindowSkin(SkinDat());
    CHECK(read.value.has_value());
    const WindowSkin& skin = *read.value;
    CHECK(skin.back.width == 128 && skin.back.height == 128);
    CHECK(skin.across.width == 32 && skin.down.width == 32 && skin.corner.width == 32);
    CHECK((First(skin.back) == std::vector<uint8_t>{255, 0, 0, 255}));
    CHECK((First(skin.across) == std::vector<uint8_t>{0, 0, 255, 255}));
    CHECK((First(skin.down) == std::vector<uint8_t>{0, 255, 0, 255}));
    CHECK((First(skin.corner) == std::vector<uint8_t>{255, 255, 255, 255}));
    // The game's colors are at half: 0x80 is full.
    CHECK_EQ(skin.shadeTop, 0.5f);
    CHECK_EQ(skin.shadeBottom, 127.0f / 128.0f);
}

TEST(a_dat_without_a_piece_is_refused_with_why)
{
    const Read<WindowSkin> read = ReadWindowSkin(SkinDat(false));
    CHECK(!read.value.has_value());
    CHECK(read.failure.find("vfr1") != std::string::npos);
    CHECK(!ReadWindowSkin(Bytes{}).value.has_value());
}

TEST(a_skin_whose_frame_is_too_small_to_cut_is_refused)
{
    // The bottom right corner is cut from 7, 7 to 31, 31.
    CHECK(!ReadWindowSkin(SkinDat(true, 0x40, 0x7F, 28)).value.has_value());
}

TEST(a_shade_past_full_is_held_to_full)
{
    const Read<WindowSkin> read = ReadWindowSkin(SkinDat(true, 0x90, 0xFF));
    CHECK(read.value.has_value());
    CHECK_EQ(read.value->shadeTop, 1.0f);
    CHECK_EQ(read.value->shadeBottom, 1.0f);
}

TEST(each_window_type_reads_its_own_file)
{
    CHECK_EQ(WindowSkinFile(0), std::string("ROM\\0\\14.DAT"));
    CHECK_EQ(WindowSkinFile(kWindowTypes - 1), std::string("ROM\\0\\21.DAT"));
}

TEST(a_skin_is_read_where_xipivot_would_read_it)
{
    CHECK((GameFileCandidates("[overlays]\n0=A\n", "D:\\d", "C:\\Game", WindowSkinFile(2)) ==
           std::vector<std::string>{"D:\\d\\A\\ROM\\0\\16.DAT", "C:\\Game\\ROM\\0\\16.DAT"}));
}

namespace
{
    // A picture whose every pixel says where it is: blue its column, green its row, opaque.
    ColorPicture Placed(int width, int height)
    {
        ColorPicture p{"", width, height, std::vector<uint8_t>(static_cast<size_t>(width * height) * 4)};
        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x)
            {
                uint8_t* pixel = p.bgra.data() + (static_cast<size_t>(y * width + x)) * 4;
                pixel[0]       = static_cast<uint8_t>(x);
                pixel[1]       = static_cast<uint8_t>(y);
                pixel[3]       = 255;
            }
        return p;
    }
    WindowSkin PlacedSkin() { return WindowSkin{Placed(128, 128), Placed(32, 32), Placed(32, 32), Placed(32, 32)}; }

    using Skins = std::array<std::optional<WindowSkin>, kWindowTypes>;
    Skins Only(int type, WindowSkin skin)
    {
        Skins skins;
        skins[static_cast<size_t>(type)] = std::move(skin);
        return skins;
    }
    // A pixel's column and row as the placed picture wrote them.
    std::pair<int, int> Where(const Image& image, int x, int y)
    {
        const uint32_t argb = image.argb[static_cast<size_t>(y * image.width + x)];
        return {static_cast<int>(argb & 0xFF), static_cast<int>(argb >> 8 & 0xFF)};
    }
}

TEST(a_corner_is_its_24_pixels_of_the_corner_picture)
{
    const Skins skins = Only(0, PlacedSkin());
    const auto corner = [&](SkinPiece piece) { return SkinImage(skins, SkinSource(0, piece, 24)); };
    for (const auto& [piece, u, v] : {std::tuple{SkinPiece::TopLeft, 1, 1}, std::tuple{SkinPiece::TopRight, 7, 1},
             std::tuple{SkinPiece::BottomLeft, 1, 7}, std::tuple{SkinPiece::BottomRight, 7, 7}})
    {
        const std::optional<Image> image = corner(piece);
        CHECK(image && image->width == 24 && image->height == 24);
        CHECK((Where(*image, 0, 0) == std::pair{u, v}));
        CHECK((Where(*image, 23, 23) == std::pair{u + 23, v + 23}));
    }
}

TEST(an_edge_is_5_pixels_thick_and_its_pictures_length)
{
    const Skins skins = Only(0, PlacedSkin());
    const std::optional<Image> top = SkinImage(skins, SkinSource(0, SkinPiece::Top, 5));
    CHECK(top && top->width == 32 && top->height == 5);
    CHECK((Where(*top, 0, 0) == std::pair{0, 1}) && (Where(*top, 31, 4) == std::pair{31, 5}));
    const std::optional<Image> bottom = SkinImage(skins, SkinSource(0, SkinPiece::Bottom, 5));
    CHECK(bottom && (Where(*bottom, 3, 0) == std::pair{3, 26}));
    const std::optional<Image> left = SkinImage(skins, SkinSource(0, SkinPiece::Left, 5));
    CHECK(left && left->width == 5 && left->height == 32 && (Where(*left, 0, 9) == std::pair{1, 9}));
    const std::optional<Image> right = SkinImage(skins, SkinSource(0, SkinPiece::Right, 5));
    CHECK(right && (Where(*right, 4, 31) == std::pair{30, 31}));
    const std::optional<Image> back = SkinImage(skins, SkinSource(0, SkinPiece::Back, 128));
    CHECK(back && back->width == 128 && (Where(*back, 127, 64) == std::pair{127, 64}));
}

TEST(a_piece_scaled_is_the_mean_of_what_each_of_its_pixels_covers)
{
    WindowSkin skin = PlacedSkin();
    for (size_t p = 0; p < skin.corner.bgra.size(); p += 4)
    {
        const size_t pixel = p / 4;
        const bool white   = (pixel % 32 + pixel / 32) % 2 == 0;
        for (size_t c = 0; c < 3; ++c)
            skin.corner.bgra[p + c] = white ? 255 : 0;
    }
    const std::optional<Image> half = SkinImage(Only(0, skin), SkinSource(0, SkinPiece::TopLeft, 12));
    CHECK(half && half->width == 12);
    for (const uint32_t argb : half->argb)
        CHECK(argb == 0xFF808080u || argb == 0xFF7F7F7Fu);
    // Half again as large: an edge's 5 rows become 8 and keep their order.
    const std::optional<Image> grown = SkinImage(Only(0, PlacedSkin()), SkinSource(0, SkinPiece::Top, 8));
    CHECK(grown && grown->height == 8 && Where(*grown, 0, 0).second == 1 && Where(*grown, 0, 7).second == 5);
}

TEST(a_skins_names_carry_its_type)
{
    const Skins skins = Only(3, PlacedSkin());
    CHECK(SkinSource(0, SkinPiece::Top, 5) != SkinSource(3, SkinPiece::Top, 5));
    CHECK(SkinImage(skins, SkinSource(3, SkinPiece::Top, 5)).has_value());
    CHECK(!SkinImage(skins, SkinSource(0, SkinPiece::Top, 5)).has_value());
}

TEST(a_name_of_no_skin_or_no_piece_is_none)
{
    const Skins skins = Only(0, PlacedSkin());
    for (const char* name : {"hu:icon/1", "hu:skin/9/top/5", "hu:skin/0/nope/5", "hu:skin/0/top/0", "hu:skin/0/top",
             "hu:skin/0/top/5/1", "hu:skin/x/top/5", "hu:skin/0/top/100000"})
        CHECK(!SkinImage(skins, name).has_value());
}

TEST(clear_is_one_clear_pixel)
{
    const std::optional<Image> clear = SkinImage(Only(0, PlacedSkin()), SkinSource(0, SkinPiece::Clear, 1));
    CHECK(clear && clear->width == 1 && clear->height == 1 && clear->argb == std::vector<uint32_t>{0});
}

TEST(sizes_are_the_menus_pixels_times_its_scale)
{
    CHECK_EQ(SkinPixels(24, 4.0f / 3.0f), 32);
    CHECK_EQ(SkinPixels(5, 4.0f / 3.0f), 7);
    CHECK_EQ(SkinPixels(5, 0.01f), 1);
}

namespace
{
    // A skin of one background color and a frame of another, at full shade.
    WindowSkin Colored(uint32_t back, uint32_t frame)
    {
        const auto fill = [](int side, uint32_t argb) {
            ColorPicture p{"", side, side, {}};
            for (int i = 0; i < side * side; ++i)
                p.bgra.insert(p.bgra.end(), {static_cast<uint8_t>(argb), static_cast<uint8_t>(argb >> 8),
                                                static_cast<uint8_t>(argb >> 16), static_cast<uint8_t>(argb >> 24)});
            return p;
        };
        return WindowSkin{fill(128, back), fill(32, frame), fill(32, frame), fill(32, frame)};
    }

    double Linear(uint32_t argb, int shift)
    {
        const double c = static_cast<double>(argb >> shift & 0xFF) / 255.0;
        return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
    }
    // WCAG's relative luminance and contrast ratio.
    double Luminance(uint32_t argb) { return 0.2126 * Linear(argb, 16) + 0.7152 * Linear(argb, 8) + 0.0722 * Linear(argb, 0); }
    double Contrast(uint32_t a, uint32_t b)
    {
        const double la = Luminance(a), lb = Luminance(b);
        return (std::max(la, lb) + 0.05) / (std::min(la, lb) + 0.05);
    }
    // A color's hue in degrees, and how far from grey it is (its largest channel less its smallest, of 255).
    double Hue(uint32_t argb)
    {
        const double r = argb >> 16 & 0xFF, g = argb >> 8 & 0xFF, b = argb & 0xFF;
        return std::fmod(std::atan2(std::sqrt(3.0) * (g - b), 2 * r - g - b) * 180.0 / std::numbers::pi + 360.0, 360.0);
    }
    int Chroma(uint32_t argb)
    {
        const int r = argb >> 16 & 0xFF, g = argb >> 8 & 0xFF, b = argb & 0xFF;
        return std::max({r, g, b}) - std::min({r, g, b});
    }
}

TEST(the_palette_keeps_the_backgrounds_hue)
{
    const uint32_t blue = 0xFF1E2A6E;
    const BarPalette p  = SkinPalette(Colored(blue, 0xFF888888));
    CHECK(std::abs(Hue(p.accent) - Hue(blue)) < 2.0);
    CHECK(Luminance(p.accent) > Luminance(blue) && Chroma(p.accent) >= Chroma(blue));
    const MenuColors m = SkinMenuColors(Colored(blue, 0xFF888888));
    CHECK(m.accent == p.accent && std::abs(Hue(m.control) - Hue(blue)) < 2.0);
}

TEST(its_text_stands_out_from_its_background)
{
    for (const uint32_t back : {0xFF101018u, 0xFF4F4836u, 0xFFE8E0D0u})
    {
        const BarPalette p = SkinPalette(Colored(back, 0xFF888888));
        const double needed = back == 0xFF4F4836u ? 4.5 : 7.0;
        CHECK(Contrast(p.text, p.panelTop) >= needed && Contrast(p.title, p.panelTop) >= needed);
        CHECK(Contrast(SkinMenuColors(Colored(back, 0xFF888888)).text, p.panelTop) >= needed);
    }
}

TEST(a_grey_skin_gives_a_grey_accent)
{
    const BarPalette p = SkinPalette(Colored(0xFF404040, 0xFF888888));
    CHECK(Chroma(p.accent) <= 2 && Luminance(p.accent) > Luminance(0xFF404040));
}

TEST(the_background_is_shaded_as_the_game_shades_it)
{
    WindowSkin skin  = Colored(0xFF8080FF, 0xFF888888);
    skin.shadeTop    = 0.5f;
    skin.shadeBottom = 1.0f;
    const BarPalette p = SkinPalette(skin);
    CHECK_EQ(p.panelTop & 0xFFFFFF, 0x404080u);
    CHECK_EQ(p.panelBottom & 0xFFFFFF, 0x8080FFu);
}

TEST(the_palette_is_the_window_shape_with_no_pills_and_no_glow)
{
    const BarPalette p = SkinPalette(Colored(0xFF1E2A6E, 0xFF888888));
    CHECK(p.shape == BarShape::Window && p.pill == 0 && p.pillBorder == 0 && p.below == 0 && p.underLine == 0);
    CHECK(p.panelBorder == 0xFF888888u);
}

TEST(a_box_is_drawn_as_the_game_draws_a_window_its_frame_over_its_shade_over_its_background)
{
    const SkinFrame frame{2, 1.5f, 0.5f, 1.0f};
    const std::string rule = SkinDecorator(frame);
    const auto at = [&](const std::string& part) { return rule.find(part); };
    // 24 menu pixels at 1.5 are 36; 5 are 8; the tile 192.
    for (const auto& [piece, pixels] : {std::pair{SkinPiece::TopLeft, 36}, std::pair{SkinPiece::Top, 8},
             std::pair{SkinPiece::TopRight, 36}, std::pair{SkinPiece::Left, 8}, std::pair{SkinPiece::Right, 8},
             std::pair{SkinPiece::BottomLeft, 36}, std::pair{SkinPiece::Bottom, 8}, std::pair{SkinPiece::BottomRight, 36}})
        CHECK(at(SkinSource(2, piece, pixels)) != std::string::npos);
    // The frame first, as RmlUi draws the first decorator on top; the background last, repeated.
    CHECK(rule.starts_with("decorator:tiled-box("));
    CHECK(at("vertical-gradient(#00000080 #00000000)") > at(SkinSource(2, SkinPiece::BottomRight, 36)));
    CHECK(at(SkinSource(2, SkinPiece::Back, 192)) > at("vertical-gradient("));
    CHECK(at("repeat") != std::string::npos && rule.ends_with(";"));
}

TEST(a_skins_look_is_its_frame_its_palette_and_its_menus_colors)
{
    WindowSkin skin  = Colored(0xFF1E2A6E, 0xFF888888);
    skin.shadeTop    = 0.25f;
    const GameSkinLook look = SkinLookOf(skin, 4, 2.0f);
    CHECK((look.frame == SkinFrame{4, 2.0f, 0.25f, 1.0f}));
    CHECK(look.palette == SkinPalette(skin) && look.menu == SkinMenuColors(skin));
}

TEST(the_skin_drawn_is_the_pinned_type_else_the_games_else_type_1)
{
    CHECK_EQ(SkinTypeOf(3, 6), 2);
    CHECK_EQ(SkinTypeOf(kFollowGameWindow, 6), 6);
    CHECK_EQ(SkinTypeOf(kFollowGameWindow, std::nullopt), 0);
    CHECK_EQ(SkinTypeOf(kWindowTypes, std::nullopt), kWindowTypes - 1);
}
