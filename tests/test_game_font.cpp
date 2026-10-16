#include "game_font.h"
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
    Bytes PictureBody(int32_t width, int32_t height, const char* code, const Bytes& blocks, uint8_t flag = 0xA1)
    {
        Bytes out{flag};
        PutText(out, kPictureName);
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
    constexpr uint16_t kWhite = 0xFFFF, kBlack = 0x0000;

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
    const Bytes blocks = Dxt3Block(alphas, kWhite, kBlack, picks) + Dxt3Block(opaque, kBlack, kWhite, {3, 2});
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
    const Bytes blocks = ColorBlock(kBlack, kWhite, {0, 1, 2, 3}) + ColorBlock(kWhite, kBlack, {3});
    const Read<FontPicture> read = ReadFontPicture(PictureBody(8, 4, "DXT1", blocks));
    CHECK(read.value);
    const std::vector<uint8_t>& fill = read.value->fill;
    CHECK(fill[0] == 0 && fill[1] == 255 && fill[2] == 127 && fill[3] == 0);
    CHECK_EQ(fill[4], 85);
}

TEST(a_record_that_is_no_picture_is_refused_with_why)
{
    const Bytes block = Dxt3Block({}, kWhite, kBlack, {});
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
    blocks = Dxt3Block(opaque, kWhite, kWhite, {});
    for (int b = 1; b < kSheetWide / 4 * kSheetTall / 4; ++b)
        blocks = blocks + Dxt3Block({}, kBlack, kBlack, {});
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
