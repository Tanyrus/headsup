#include "game_font.h"
#include "plate_image.h"
#include "plate_settings.h"
#include "test.h"

#include <array>
#include <cstring>
#include <string>
#include <vector>

using namespace headsup;

namespace
{
    using Bytes = std::vector<uint8_t>;

    constexpr const char* kPictureName = "font    font    ";

    void Put16(Bytes& out, uint16_t v) { out.insert(out.end(), {static_cast<uint8_t>(v), static_cast<uint8_t>(v >> 8)}); }
    void Put32(Bytes& out, uint32_t v)
    {
        for (int i = 0; i < 4; ++i)
            out.push_back(static_cast<uint8_t>(v >> (8 * i)));
    }
    void PutText(Bytes& out, const char* text) { out.insert(out.end(), text, text + std::strlen(text)); }
    Bytes operator+(Bytes a, const Bytes& b)
    {
        a.insert(a.end(), b.begin(), b.end());
        return a;
    }

    // A DAT chunk: its name, a word of its type (low 7 bits) and its size in 16-byte units (the next 19), 8 bytes
    // unused, then the body padded to 16 bytes.
    Bytes Chunk(const char* name, uint32_t type, Bytes body)
    {
        body.resize((body.size() + 15) / 16 * 16);
        Bytes out(name, name + 4);
        Put32(out, static_cast<uint32_t>((16 + body.size()) / 16) << 7 | type);
        out.resize(16);
        return out + body;
    }

    // A picture chunk's body as 51.DAT has it: 0xA1, a 16-byte name, a BITMAPINFOHEADER, the four-letter code stored
    // backwards, the data's size and a dword, then the blocks.
    Bytes PictureBody(int32_t width, int32_t height, const char* code, const Bytes& blocks, uint8_t flag = 0xA1,
        const char* name = kPictureName)
    {
        Bytes out{flag};
        PutText(out, name);
        Put32(out, 40);
        Put32(out, static_cast<uint32_t>(width));
        Put32(out, static_cast<uint32_t>(height));
        Put16(out, 1);
        Put16(out, 32);
        out.resize(out.size() + 24);
        for (int i = 3; i >= 0; --i)
            out.push_back(static_cast<uint8_t>(code[i]));
        Put32(out, static_cast<uint32_t>(blocks.size()));
        Put32(out, 0x400);
        return out + blocks;
    }

    // Direct3D's blocks of 4 x 4 pixels, the first pixel in the lowest bits: DXT3 is 16 alphas of 4 bits, then a DXT1
    // color block, two 565 colors and a 2-bit pick a pixel.
    Bytes ColorBlock(uint16_t c0, uint16_t c1, const std::array<uint8_t, 16>& picks)
    {
        Bytes out;
        Put16(out, c0);
        Put16(out, c1);
        uint32_t bits = 0;
        for (int k = 0; k < 16; ++k)
            bits |= static_cast<uint32_t>(picks[static_cast<size_t>(k)]) << (2 * k);
        Put32(out, bits);
        return out;
    }
    Bytes Dxt3Block(const std::array<uint8_t, 16>& alphas, uint16_t c0, uint16_t c1, const std::array<uint8_t, 16>& picks)
    {
        Bytes out(8, 0);
        for (int k = 0; k < 16; ++k)
            out[static_cast<size_t>(k / 2)] |= static_cast<uint8_t>(alphas[static_cast<size_t>(k)] << (k % 2 * 4));
        return out + ColorBlock(c0, c1, picks);
    }
    constexpr uint16_t kWhite565 = 0xFFFF, kBlack565 = 0x0000;

    // A shapes chunk's body as 51.DAT has it: its name, the pictures it draws from, the shape count, then each shape:
    // a flag, its box's four corners, the picture's rectangle as width, height, u and v, a byte and four colors; every
    // shape after the first opens with four bytes and the name of its picture.
    Bytes ShapesBody(const std::vector<GlyphShape>& shapes, const char* picture = kPictureName, uint8_t flag = 1)
    {
        Bytes out;
        PutText(out, "font    fontshp ");
        out.push_back(2);
        PutText(out, "MENU    ustatsHD");
        PutText(out, kPictureName);
        Put16(out, static_cast<uint16_t>(shapes.size()));
        for (size_t i = 0; i < shapes.size(); ++i)
        {
            const GlyphShape& s = shapes[i];
            if (i > 0)
            {
                out.insert(out.end(), {1, 0, 1, 1});
                PutText(out, picture);
            }
            out.push_back(flag);
            for (const int v : {s.left, s.top, s.right, s.top, s.left, s.bottom, s.right, s.bottom, s.width, s.height, s.u, s.v})
                Put16(out, static_cast<uint16_t>(v));
            out.push_back(0);
            out.insert(out.end(), 16, 0x7F);
        }
        return out;
    }

    // A glyph for each character: its box and rectangle differ from the others' so a shape read for another fails.
    std::vector<GlyphShape> Glyphs(size_t count = kGlyphCount)
    {
        std::vector<GlyphShape> shapes;
        for (size_t i = 0; i < count; ++i)
        {
            const int n = static_cast<int>(i);
            shapes.push_back(GlyphShape{0, n % 3, 4 + n % 6, 11 + n % 3, 2 * n, 1 + n % 5, 2 + n % 3, 3 + n % 4});
        }
        return shapes;
    }
    constexpr int kSheetWide = 256, kSheetTall = 16;

    // A face whose picture is clear but for the rectangles a test fills, every glyph a box of 4 x 11 units drawn from
    // a clear corner, and the space 4 x 8.
    GameFontFace Face(int width = 64, int height = 32)
    {
        GameFontFace face;
        face.picture = FontPicture{kPictureName, width, height, std::vector<uint8_t>(static_cast<size_t>(width * height), 0)};
        for (GlyphShape& g : face.glyphs)
            g = GlyphShape{0, 0, 4, 11, 60, 0, 4, 11};
        face.glyphs[0] = GlyphShape{0, 0, 4, 8, 60, 0, 4, 8};
        return face;
    }
    // A glyph drawn from a width by height rectangle at u, v, all of it filled at alpha, into a box of the same size
    // in units.
    void SetGlyph(GameFontFace& face, char c, int u, int v, int width, int height, uint8_t alpha = 255)
    {
        face.glyphs[static_cast<size_t>(c - kFirstGlyph)] = GlyphShape{0, 0, width, height, u, v, width, height};
        for (int y = v; y < v + height; ++y)
            for (int x = u; x < u + width; ++x)
                face.picture.fill[static_cast<size_t>(y * face.picture.width + x)] = alpha;
    }
    uint8_t At(const Coverage& c, int x, int y) { return c.alpha[static_cast<size_t>(y * c.width + x)]; }
    // The columns of a coverage's row that hold anything.
    std::vector<int> InkColumns(const Coverage& c, int y)
    {
        std::vector<int> columns;
        for (int x = 0; x < c.width; ++x)
            if (At(c, x, y) != 0) columns.push_back(x);
        return columns;
    }
    constexpr uint8_t kPlain = 0;

    bool Same(const GlyphShape& a, const GlyphShape& b)
    {
        return a.left == b.left && a.top == b.top && a.right == b.right && a.bottom == b.bottom && a.u == b.u && a.v == b.v &&
               a.width == b.width && a.height == b.height;
    }
}

TEST(the_game_fonts_are_known_by_name_in_any_case)
{
    CHECK(GameFontVariantOf(kGameFontName) == GameFontVariant::Game);
    CHECK(GameFontVariantOf("game font narrow") == GameFontVariant::Narrow);
    CHECK(!GameFontVariantOf("Game Fonts"));
    CHECK(!GameFontVariantOf("Montserrat SemiBold"));
}

TEST(a_dat_is_walked_chunk_by_chunk_to_the_one_of_that_name_and_type)
{
    Bytes dat = Chunk("menu", 0x01, Bytes(16, 7)) + Chunk("font", 0x20, Bytes(40, 9)) + Chunk("font", 0x31, Bytes(20, 5));
    const auto picture = FindChunk(dat, "font", kPictureChunkType);
    CHECK(picture && picture->data() == dat.data() + 32 + 16 && picture->size() == 48u);
    const auto shapes = FindChunk(dat, "font", kShapesChunkType);
    CHECK(shapes && shapes->data() == dat.data() + 96 + 16 && shapes->size() == 32u);
    CHECK(!FindChunk(dat, "moji", kPictureChunkType));
    // A size that runs past the file ends the walk.
    dat[96 + 5] = 0x40;
    CHECK(!FindChunk(dat, "font", kShapesChunkType));
    CHECK(!FindChunk(Bytes(10, 0), "font", kPictureChunkType));
}

TEST(a_dxt3_pictures_letters_are_their_white_times_their_alpha)
{
    // DXT3 always has four colors: the two given, and two thirds and one third of the way from the first to the
    // second. The picture draws white letters inside a black edge: the black is left out.
    std::array<uint8_t, 16> alphas{};
    alphas.fill(15);
    alphas[3] = 5;
    alphas[4] = 0;
    const std::array<uint8_t, 16> picks{0, 1, 2, 0, 0, 3};
    std::array<uint8_t, 16> opaque{};
    opaque.fill(15);
    // The second block's first color is the smaller, which in DXT1 would make its fourth clear.
    const Bytes blocks = Dxt3Block(alphas, kWhite565, kBlack565, picks) + Dxt3Block(opaque, kBlack565, kWhite565, {3, 2});
    const Read<FontPicture> read = ReadFontPicture(PictureBody(8, 4, "DXT3", blocks));
    CHECK(read.value && read.failure.empty());
    CHECK(read.value->name == kPictureName && read.value->width == 8 && read.value->height == 4);
    const std::vector<uint8_t>& fill = read.value->fill;
    CHECK((std::vector<uint8_t>(fill.begin(), fill.begin() + 6) == std::vector<uint8_t>{255, 0, 170, 85, 170, 85}));
    CHECK((std::vector<uint8_t>(fill.begin() + 8, fill.begin() + 12) == std::vector<uint8_t>{0, 85, 255, 255}));
}

TEST(a_dxt1_picture_is_clear_where_it_picks_its_fourth_color_after_a_smaller_first)
{
    // DXT1 with the first color not above the second has three: the two and halfway, and its fourth is clear.
    const Bytes blocks = ColorBlock(kBlack565, kWhite565, {0, 1, 2, 3}) + ColorBlock(kWhite565, kBlack565, {3});
    const Read<FontPicture> read = ReadFontPicture(PictureBody(8, 4, "DXT1", blocks));
    CHECK(read.value);
    const std::vector<uint8_t>& fill = read.value->fill;
    CHECK(fill[0] == 0 && fill[1] == 255 && fill[2] == 127 && fill[3] == 0);
    CHECK_EQ(fill[4], 85);
}

TEST(a_pictures_colors_are_read_blue_green_red_alpha_a_pixel)
{
    // Red and blue at full, and DXT3's two thirds of the way each side; its alphas are four bits times 17.
    const std::array<uint8_t, 16> alphas{15, 8, 0, 4};
    const Bytes dxt3 = Dxt3Block(alphas, 0xF800, 0x001F, {0, 1, 2, 3}) + Dxt3Block({}, 0xF800, 0x001F, {});
    const Read<ColorPicture> read = ReadColorPicture(PictureBody(8, 4, "DXT3", dxt3));
    CHECK(read.value && read.failure.empty());
    CHECK(read.value->name == kPictureName && read.value->width == 8 && read.value->height == 4);
    CHECK((std::vector<uint8_t>(read.value->bgra.begin(), read.value->bgra.begin() + 16) ==
           std::vector<uint8_t>{0, 0, 255, 255, 255, 0, 0, 136, 85, 0, 170, 0, 170, 0, 85, 68}));
    // DXT1 with the first color not above the second: halfway for the third pick, clear for the fourth.
    const Bytes dxt1             = ColorBlock(0x001F, 0xF800, {0, 2, 3}) + ColorBlock(0x07E0, 0, {});
    const Read<ColorPicture> one = ReadColorPicture(PictureBody(8, 4, "DXT1", dxt1));
    CHECK(one.value);
    CHECK((std::vector<uint8_t>(one.value->bgra.begin(), one.value->bgra.begin() + 12) ==
           std::vector<uint8_t>{255, 0, 0, 255, 127, 0, 127, 255, 0, 0, 0, 0}));
    CHECK((std::vector<uint8_t>(one.value->bgra.begin() + 16, one.value->bgra.begin() + 20) == std::vector<uint8_t>{0, 255, 0, 255}));
    CHECK(!ReadColorPicture(PictureBody(4, 4, "DXT5", dxt3)).value);
}

TEST(the_games_pearl_is_the_square_at_0_64_of_its_status_icons_picture)
{
    // 256 x 256 in DXT1: green in the 8 x 8 blocks from block (0, 16), red elsewhere.
    Bytes blocks;
    for (int by = 0; by < 64; ++by)
        for (int bx = 0; bx < 64; ++bx)
            blocks = blocks + ColorBlock(bx < 8 && by >= 16 && by < 24 ? 0x07E0 : 0xF800, 0, {});
    const Bytes status = Chunk("usta", 0x20, PictureBody(256, 256, "DXT1", blocks, 0xA1, "menu    ustatshd"));
    const Bytes other  = Chunk("font", 0x20, PictureBody(4, 4, "DXT1", ColorBlock(0xFFFF, 0, {})));
    const Read<ColorPicture> pearl = ReadGamePearl(other + status);
    CHECK(pearl.value && pearl.failure.empty());
    CHECK(pearl.value->width == 32 && pearl.value->height == 32);
    bool green = pearl.value->bgra.size() == 32 * 32 * 4;
    for (size_t i = 0; green && i < pearl.value->bgra.size(); i += 4)
        green = pearl.value->bgra[i] == 0 && pearl.value->bgra[i + 1] == 255 && pearl.value->bgra[i + 2] == 0 &&
                pearl.value->bgra[i + 3] == 255;
    CHECK(green);
    CHECK(!ReadGamePearl(other).value && !ReadGamePearl(other).failure.empty());
    const Bytes small = Chunk("usta", 0x20, PictureBody(32, 32, "DXT1", Bytes(64 * 8, 0), 0xA1, "menu    ustatshd"));
    CHECK(!ReadGamePearl(small).value && !ReadGamePearl(small).failure.empty());
    const Bytes renamed = Chunk("usta", 0x20, PictureBody(256, 256, "DXT1", blocks, 0xA1, "menu    ustatsxx"));
    CHECK(!ReadGamePearl(renamed).value);
    // DXT1's fourth pick after a smaller first color is clear: a pearl's square all of it shows nothing.
    Bytes clear;
    for (int by = 0; by < 64; ++by)
        for (int bx = 0; bx < 64; ++bx)
        {
            std::array<uint8_t, 16> picks{};
            if (bx < 8 && by >= 16 && by < 24) picks.fill(3);
            clear = clear + ColorBlock(0, 0x07E0, picks);
        }
    const Read<ColorPicture> nothing = ReadGamePearl(Chunk("usta", 0x20, PictureBody(256, 256, "DXT1", clear, 0xA1, "menu    ustatshd")));
    CHECK(!nothing.value && !nothing.failure.empty());
}

TEST(a_record_that_is_no_picture_is_refused_with_why)
{
    const Bytes block = Dxt3Block({}, kWhite565, kBlack565, {});
    auto refused      = [](const Bytes& body) {
        const Read<FontPicture> read = ReadFontPicture(body);
        return !read.value && !read.failure.empty();
    };
    CHECK(!refused(PictureBody(4, 4, "DXT3", block)));
    CHECK(refused(PictureBody(4, 4, "DXT3", block, 0x91)));
    CHECK(refused(PictureBody(4, 4, "DXT5", block)));
    CHECK(refused(PictureBody(6, 4, "DXT3", block + block)));
    CHECK(refused(PictureBody(0, 4, "DXT3", block)));
    CHECK(refused(PictureBody(kMaxPictureSide + 4, 4, "DXT3", Bytes(static_cast<size_t>(kMaxPictureSide + 4) * 4, 0))));
    CHECK(refused(PictureBody(8, 4, "DXT3", block)));
    CHECK(refused(Bytes(30, 0)));
}

TEST(the_shapes_give_each_characters_box_and_rectangle_from_the_space_to_the_tilde)
{
    const std::vector<GlyphShape> glyphs = Glyphs();
    const auto read = ReadGlyphShapes(ShapesBody(glyphs), kPictureName, kSheetWide, kSheetTall);
    CHECK(read.value && read.failure.empty());
    bool every = true;
    for (size_t i = 0; i < kGlyphCount; ++i)
        every = every && Same((*read.value)[i], glyphs[i]);
    CHECK(every);
    // Shapes past the tilde, the font's own extras and icons from other pictures, are not read.
    std::vector<GlyphShape> more = glyphs;
    more.push_back(GlyphShape{0, -2, 16, 14, 0, 96, 32, 32});
    CHECK(ReadGlyphShapes(ShapesBody(more), kPictureName, kSheetWide, kSheetTall).value);
}

TEST(shapes_that_are_not_the_fonts_glyphs_are_refused_with_why)
{
    auto refused = [](const Bytes& body, int width = kSheetWide) {
        const auto read = ReadGlyphShapes(body, kPictureName, width, kSheetTall);
        return !read.value && !read.failure.empty();
    };
    CHECK(refused(ShapesBody(Glyphs(kGlyphCount - 1))));
    CHECK(refused(ShapesBody(Glyphs(), "MENU    ustatsHD")));
    CHECK(refused(ShapesBody(Glyphs(), kPictureName, 2)));
    CHECK(refused(ShapesBody(Glyphs()), 100)); // a rectangle past the picture's edge
    std::vector<GlyphShape> flat = Glyphs();
    flat[10].bottom              = flat[10].top;
    CHECK(refused(ShapesBody(flat)));
    const Bytes body = ShapesBody(Glyphs());
    CHECK(refused(Bytes(body.begin(), body.end() - 20)));
    CHECK(refused(Bytes(body.begin(), body.begin() + 40)));
}

TEST(the_font_is_its_picture_and_shapes_out_of_one_dat)
{
    // A picture 256 x 16 of clear blocks, but for the first, which is white.
    Bytes blocks;
    std::array<uint8_t, 16> opaque{};
    opaque.fill(15);
    blocks = Dxt3Block(opaque, kWhite565, kWhite565, {});
    for (int b = 1; b < kSheetWide / 4 * kSheetTall / 4; ++b)
        blocks = blocks + Dxt3Block({}, kBlack565, kBlack565, {});
    const Bytes picture = Chunk("font", kPictureChunkType, PictureBody(kSheetWide, kSheetTall, "DXT3", blocks));
    const Bytes shapes  = Chunk("font", kShapesChunkType, ShapesBody(Glyphs()));
    const Read<GameFontFace> read = ReadGameFont(Chunk("menu", 0x01, Bytes(16, 0)) + picture + shapes);
    CHECK(read.value && read.failure.empty());
    CHECK(read.value->picture.width == kSheetWide && read.value->picture.fill[0] == 255 && read.value->picture.fill[4] == 0);
    CHECK(Same(read.value->glyphs['u' - kFirstGlyph], Glyphs()['u' - kFirstGlyph]));
    CHECK(!ReadGameFont(picture).value && !ReadGameFont(picture).failure.empty());
    CHECK(!ReadGameFont(shapes).value);
    CHECK(!ReadGameFont(picture + Chunk("font", kShapesChunkType, ShapesBody(Glyphs(), "font    other   "))).value);
}

TEST(the_font_is_read_where_xipivot_would_read_it_then_from_the_game)
{
    // XIPivot's overlays, numbered from 0 until one is missing, each under its root; the lowest number wins.
    const std::string game = "C:\\Game\\ROM\\119\\51.DAT";
    CHECK((GameFontCandidates("[settings]\r\nroot_path=X:\\r\r\n[overlays]\r\n0=A\r\n1=B\r\n3=D\r\n", "D:\\d", "C:\\Game") ==
           std::vector<std::string>{"X:\\r\\A\\ROM\\119\\51.DAT", "X:\\r\\B\\ROM\\119\\51.DAT", game}));
    CHECK((GameFontCandidates("[Overlays]\n0 = A\n[SETTINGS]\nRoot_Path=X:\\r\\\n", "D:\\d", "C:\\Game") ==
           std::vector<std::string>{"X:\\r\\A\\ROM\\119\\51.DAT", game}));
    CHECK((GameFontCandidates("[overlays]\n0=A\n", "D:\\d", "C:\\Game") == std::vector<std::string>{"D:\\d\\A\\ROM\\119\\51.DAT", game}));
    CHECK((GameFontCandidates("[settings]\nroot_path=X:\\r\n", "D:\\d", "C:\\Game") == std::vector<std::string>{game}));
    CHECK((GameFontCandidates("", "D:\\d", "C:\\Game") == std::vector<std::string>{game}));
    // This install's file.
    const char* pivot = "[settings]\nroot_path=X:\\Games\\PhoenixXI\\polplugins\\DATs\ndebug_log=false\nredirect_fopens=true\n"
                        "[overlays]\n0=EraDATs\n1=XICombinedMaps-2048\n2=XiView-Standard\n";
    const std::vector<std::string> files = GameFontCandidates(pivot, "D:\\d", "C:\\Game");
    CHECK(files.size() == 4u && files[2] == "X:\\Games\\PhoenixXI\\polplugins\\DATs\\XiView-Standard\\ROM\\119\\51.DAT");
}

TEST(a_character_past_the_fonts_is_drawn_as_its_question_mark)
{
    CHECK_EQ(GlyphOf(U' '), 0u);
    CHECK_EQ(GlyphOf(U'A'), static_cast<size_t>('A' - 0x20));
    CHECK_EQ(GlyphOf(U'~'), kGlyphCount - 1);
    CHECK_EQ(GlyphOf(U'\u00E9'), static_cast<size_t>('?' - 0x20));
    CHECK_EQ(GlyphOf(U'\t'), static_cast<size_t>('?' - 0x20));
}

TEST(text_is_as_wide_as_its_boxes_at_twelve_units_to_the_height_asked_and_the_game_stretches_it_16_to_9)
{
    // "AB A": boxes of 9, 6, 4 (the space) and 9 units, 28 in all; at 24 pixels a unit is 2 pixels tall, and the line
    // is 13 units.
    GameFontFace face = Face();
    SetGlyph(face, 'A', 0, 0, 9, 11);
    SetGlyph(face, 'B', 10, 0, 6, 11);
    int width = 0, height = 0;
    GameFontExtent(face, GameFontVariant::Narrow, "AB A", 24, kPlain, width, height);
    CHECK(width == 56 && height == 26);
    GameFontExtent(face, GameFontVariant::Game, "AB A", 24, kPlain, width, height);
    CHECK(width == 100 && height == 26); // 56 x 16 / 9 = 99.6
    GameFontExtent(face, GameFontVariant::Narrow, "AB A", 24, StyleBit(FontStyle::Italic), width, height);
    CHECK_EQ(width, 56 + ItalicLean(24, StyleBit(FontStyle::Italic)));
}

TEST(each_glyph_is_drawn_from_its_rectangle_into_its_box_one_after_another_inside_the_margin)
{
    // At 12 pixels a unit is a pixel: the picture's pixels land as they are.
    GameFontFace face = Face();
    SetGlyph(face, 'A', 0, 0, 3, 5);
    SetGlyph(face, 'B', 10, 0, 2, 4, 128);
    face.picture.fill[static_cast<size_t>(1 * face.picture.width + 11)] = 64;
    const Coverage c = GameFontRasterize(face, GameFontVariant::Narrow, "AB", 12, kPlain, 0, 2);
    CHECK(c.width == 3 + 2 + 4 && c.height == 13 + 4);
    CHECK((InkColumns(c, 2) == std::vector<int>{2, 3, 4, 5, 6}));
    CHECK(At(c, 2, 2) == 255 && At(c, 4, 6) == 255 && At(c, 5, 6) == 0 && At(c, 2, 7) == 0);
    // A glyph's brightest pixel is full: the font keeps its small letters, the punctuation, at about 60%.
    CHECK(At(c, 5, 2) == 255 && At(c, 6, 3) == 128);
    CHECK(InkColumns(c, 0).empty() && InkColumns(c, 1).empty());
    // Tracking adds after each character, as GDI's does.
    CHECK_EQ(GameFontRasterize(face, GameFontVariant::Narrow, "AB", 12, kPlain, 3, 2).width, 3 + 2 + 6 + 4);
    // A character the font lacks is its question mark.
    SetGlyph(face, '?', 20, 0, 3, 7);
    CHECK(GameFontRasterize(face, GameFontVariant::Narrow, "\xC3\xA9", 12, kPlain, 0, 0).alpha ==
          GameFontRasterize(face, GameFontVariant::Narrow, "?", 12, kPlain, 0, 0).alpha);
}

TEST(the_game_variant_draws_each_glyph_16_to_9_as_wide)
{
    GameFontFace face = Face();
    SetGlyph(face, 'A', 0, 0, 9, 11);
    const Coverage c = GameFontRasterize(face, GameFontVariant::Game, "A", 12, kPlain, 0, 0);
    CHECK_EQ(c.width, 16);
    CHECK((InkColumns(c, 5) == std::vector<int>{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}));
    CHECK_EQ(At(c, 8, 5), 255);
}

TEST(a_glyph_drawn_larger_is_sharpened_to_an_edge_a_pixel_wide)
{
    // Twice as large, a hard edge between two source pixels is a quarter and three quarters across the two pixels it
    // falls between; the curve makes them clear and full. At its own size nothing changes.
    CHECK_EQ(Sharpened(0.25f, 2.0f), 0);
    CHECK_EQ(Sharpened(0.75f, 2.0f), 255);
    CHECK_EQ(Sharpened(0.25f, 1.0f), 64);
    CHECK_EQ(Sharpened(0.6f, 0.5f), 153);
    GameFontFace face = Face();
    SetGlyph(face, 'A', 5, 0, 4, 11); // a block from column 5 of a 9-wide glyph
    face.glyphs['A' - kFirstGlyph] = GlyphShape{0, 0, 9, 11, 0, 0, 9, 11};
    const Coverage c = GameFontRasterize(face, GameFontVariant::Narrow, "A", 24, kPlain, 0, 0);
    CHECK(At(c, 9, 10) == 0 && At(c, 10, 10) == 255);
}

TEST(bold_strikes_a_glyph_twice_half_a_unit_apart_and_italic_leans_its_top_by_the_renderers_room)
{
    GameFontFace face = Face();
    face.glyphs['I' - kFirstGlyph] = GlyphShape{0, 0, 5, 11, 2, 0, 5, 11};
    for (int y = 0; y < 11; ++y)
        face.picture.fill[static_cast<size_t>(y * face.picture.width + 4)] = 255; // a stem at the box's column 2
    CHECK((InkColumns(GameFontRasterize(face, GameFontVariant::Narrow, "I", 24, kPlain, 0, 0), 10) == std::vector<int>{4, 5}));
    CHECK((InkColumns(GameFontRasterize(face, GameFontVariant::Narrow, "I", 24, StyleBit(FontStyle::Bold), 0, 0), 10) ==
           std::vector<int>{4, 5, 6}));
    const uint8_t italic = StyleBit(FontStyle::Italic);
    const Coverage leaning = GameFontRasterize(face, GameFontVariant::Narrow, "I", 24, italic, 0, 0);
    const int lean         = ItalicLean(24, italic);
    CHECK_EQ(leaning.width, 10 + lean);
    CHECK((InkColumns(leaning, 0) == std::vector<int>{4 + lean, 5 + lean}));
    CHECK((InkColumns(leaning, 25) == std::vector<int>{}));
    // Row 21 of the 26 leans 4/25 of the way: 0.96 of a pixel, rounded to 1.
    CHECK((InkColumns(leaning, 21) == std::vector<int>{5, 6}));
}

TEST(an_underline_runs_a_unit_under_the_baseline_a_unit_thick_and_the_renderer_draws_it)
{
    int top = 0, thickness = 0;
    GameFontUnderlineBand(24, top, thickness);
    CHECK(top == 21 && thickness == 2); // 10.5 and 1 units of 2 pixels
    // The letters come without it, as GDI's do: the renderer draws one line under the whole text.
    GameFontFace face = Face();
    SetGlyph(face, 'A', 0, 0, 9, 11);
    const Coverage c = GameFontRasterize(face, GameFontVariant::Narrow, "A", 24, StyleBit(FontStyle::Underline), 0, 1);
    CHECK(At(c, 1, 1 + 22) == 0 && At(c, 18, 1 + 22) == 0); // the band's row past the letter's last
    CHECK(c.alpha == GameFontRasterize(face, GameFontVariant::Narrow, "A", 24, kPlain, 0, 1).alpha);
}

TEST(a_glyph_drawn_smaller_averages_the_pixels_each_pixel_covers)
{
    // At half size each pixel drawn covers two of the picture's and a tent twice as wide weighs them, so a stem a
    // pixel wide keeps its weight, a quarter and three quarters of half across two pixels, where a bilinear sample
    // would drop or double it.
    GameFontFace face             = Face();
    face.glyphs['I' - kFirstGlyph] = GlyphShape{0, 0, 4, 8, 0, 0, 4, 8};
    for (int y = 0; y < 8; ++y)
        face.picture.fill[static_cast<size_t>(y * face.picture.width + 2)] = 255;
    const Coverage c = GameFontRasterize(face, GameFontVariant::Narrow, "I", 6, kPlain, 0, 0);
    CHECK(c.width == 2 && At(c, 0, 1) == 32 && At(c, 1, 1) == 96);
}
