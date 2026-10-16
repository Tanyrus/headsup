#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace headsup
{
    // Nameplate fonts drawn from the font the game writes its names in, read from the player's install: the letters
    // as the game draws them, or at their own width.
    constexpr const char* kGameFontName       = "Game Font";
    constexpr const char* kGameFontNarrowName = "Game Font Narrow";
    enum class GameFontVariant : uint8_t
    {
        Game,
        Narrow,
    };
    // Which of the two a font name is, in any case, as GDI matches names; none for any other font.
    std::optional<GameFontVariant> GameFontVariantOf(std::string_view family);

    // The font is two chunks of ROM/119/51.DAT named "font": its picture, and its shapes, the first of them one a
    // character from the space to the tilde, each a box in name units and the rectangle of the picture drawn in it.
    constexpr std::string_view kFontChunk = "font";
    constexpr uint8_t kPictureChunkType   = 0x20;
    constexpr uint8_t kShapesChunkType    = 0x31;
    constexpr char32_t kFirstGlyph        = 0x20;
    constexpr char32_t kLastGlyph         = 0x7E;
    constexpr size_t kGlyphCount          = kLastGlyph - kFirstGlyph + 1;
    constexpr int kMaxPictureSide         = 4096;

    struct GlyphShape
    {
        int left = 0, top = 0, right = 0, bottom = 0; // the box it is drawn in, in name units from the line's top
        int u = 0, v = 0, width = 0, height = 0;       // the pixels of the picture drawn into the box
    };

    struct FontPicture
    {
        std::string name; // the 16-byte name the shapes call it by
        int width = 0, height = 0;
        // The letters' coverage, one byte a pixel, rows top to bottom: their white, without the dark edge the picture
        // draws around them, which the nameplate's own shadow and glow stand in for.
        std::vector<uint8_t> fill;
    };

    struct GameFontFace
    {
        FontPicture picture;
        std::array<GlyphShape, kGlyphCount> glyphs{};
    };

    // The body of a DAT's first chunk of this name and type, past its header; none when the walk finds none.
    std::optional<std::span<const uint8_t>> FindChunk(std::span<const uint8_t> dat, std::string_view name, uint8_t type);

    // What was read, or why nothing was: for the chat line.
    template <typename T>
    struct Read
    {
        std::optional<T> value;
        std::string failure;
    };
    // A picture chunk's DXT3 or DXT1 picture.
    Read<FontPicture> ReadFontPicture(std::span<const uint8_t> body);
    // The glyphs of a shapes chunk, each drawn from the picture of that name, which is width by height.
    Read<std::array<GlyphShape, kGlyphCount>> ReadGlyphShapes(std::span<const uint8_t> body, std::string_view picture, int width,
        int height);
    // The font out of a whole 51.DAT.
    Read<GameFontFace> ReadGameFont(std::span<const uint8_t> dat);

    // The files the game reads the font from, in the order XIPivot looks: each overlay its config names (pivotIni,
    // empty without pivot) under its root_path or defaultRoot, the lowest number first, then the game's own.
    std::vector<std::string> GameFontCandidates(std::string_view pivotIni, std::string_view defaultRoot, std::string_view gameFolder);
}
