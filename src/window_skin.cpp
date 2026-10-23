#include "window_skin.h"

#include <algorithm>
#include <utility>

namespace headsup
{
    namespace
    {
        constexpr int kFirstSkinFile         = 14;
        constexpr const char* kSkinFolder    = "ROM\\0\\";
        constexpr const char* kBackChunk     = "newt";
        constexpr const char* kAcrossChunk   = "hfr1";
        constexpr const char* kDownChunk     = "vfr1";
        constexpr const char* kCornerChunk   = "corn";
        constexpr const char* kShapesChunk   = "win0";

        // The shapes chunk's body: its 16-byte name, a count of the pictures it draws from and their 16-byte names, a
        // shape count, then the shapes, the background first: a flag, twelve int16 corners and rectangle, a byte, and
        // a color a corner as four bytes, top left, top right, bottom left, bottom right.
        constexpr size_t kNameBytes      = 16;
        constexpr size_t kPicturesAt     = kNameBytes;
        constexpr size_t kShapeColorsAt  = 1 + 12 * sizeof(int16_t) + 1;
        constexpr size_t kColorBytes     = 4;
        constexpr size_t kTopLeftColor   = 0, kBottomLeftColor = 2;
        constexpr size_t kShapeBytes     = kShapeColorsAt + 4 * kColorBytes;
        // The game's colors are at half: 0x80 is the picture as it is.
        constexpr float kFullLevel = 128.0f;

        float LevelOf(const uint8_t* color)
        {
            return std::min(1.0f, static_cast<float>(std::max({color[0], color[1], color[2]})) / kFullLevel);
        }
    }

    std::string WindowSkinFile(int type) { return kSkinFolder + std::to_string(kFirstSkinFile + type) + ".DAT"; }

    Read<WindowSkin> ReadWindowSkin(std::span<const uint8_t> dat)
    {
        WindowSkin skin;
        for (const auto& [chunk, picture] : {std::pair{kBackChunk, &skin.back}, std::pair{kAcrossChunk, &skin.across},
                 std::pair{kDownChunk, &skin.down}, std::pair{kCornerChunk, &skin.corner}})
        {
            const auto body = FindChunk(dat, chunk, kPictureChunkType);
            if (!body) return {std::nullopt, std::string("it holds no ") + chunk};
            Read<ColorPicture> read = ReadColorPicture(*body);
            if (!read.value) return {std::nullopt, std::string(chunk) + ": " + read.failure};
            *picture = std::move(*read.value);
        }
        const auto shapes = FindChunk(dat, kShapesChunk, kShapesChunkType);
        if (!shapes || shapes->size() <= kPicturesAt) return {std::nullopt, "it holds no shapes"};
        const size_t first = kPicturesAt + 1 + (*shapes)[kPicturesAt] * kNameBytes + sizeof(uint16_t);
        if (shapes->size() < first + kShapeBytes) return {std::nullopt, "its shapes end early"};
        const uint8_t* colors = shapes->data() + first + kShapeColorsAt;
        skin.shadeTop         = LevelOf(colors + kTopLeftColor * kColorBytes);
        skin.shadeBottom      = LevelOf(colors + kBottomLeftColor * kColorBytes);
        return {std::move(skin), {}};
    }
}
