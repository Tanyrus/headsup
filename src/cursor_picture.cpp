#include "cursor_picture.h"

#include "argb.h"

#include <algorithm>
#include <cctype>
#include <string_view>

namespace headsup
{
    namespace
    {
        constexpr size_t kBytesPerPixel = 4;
        constexpr float kPictureTip     = 0.5f;

        Image PictureOf(const CursorImage& cursor)
        {
            Image picture{static_cast<int>(cursor.width), static_cast<int>(cursor.height), {}};
            picture.argb.reserve(cursor.bgra.size() / kBytesPerPixel);
            for (size_t at = 0; at + kBytesPerPixel <= cursor.bgra.size(); at += kBytesPerPixel)
                picture.argb.push_back(Argb(cursor.bgra[at + 3], cursor.bgra[at + 2], cursor.bgra[at + 1], cursor.bgra[at]));
            return picture;
        }
    }

    Image ChocoboPicture() { return PictureOf(ChocoboPointer().front().image); }

    std::optional<Image> CursorFilePicture(const uint8_t* data, size_t size)
    {
        const std::vector<CursorFrame> frames = ReadCursorFile(data, size);
        return frames.empty() ? std::nullopt : std::optional<Image>(PictureOf(frames.front().image));
    }

    std::vector<std::string> CursorFileNames(const std::vector<std::string>& entries)
    {
        const auto lower    = [](char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); };
        const auto lowered  = [&](std::string text) {
            std::ranges::transform(text, text.begin(), lower);
            return text;
        };
        std::vector<std::string> names;
        for (const std::string& entry : entries)
        {
            for (const char* extension : kCursorExtensions)
            {
                const std::string_view ending(extension);
                if (entry.size() <= ending.size() || lowered(entry.substr(entry.size() - ending.size())) != ending) continue;
                std::string name = entry.substr(0, entry.size() - ending.size());
                // One the settings would not keep is not offered: picking it would only put the feather back.
                if (IsCursorFileName(name)) names.push_back(std::move(name));
            }
        }
        // Windows opens a file whatever the case of its name: two that differ only by that are one name.
        std::ranges::sort(names, [&](const std::string& a, const std::string& b) {
            return lowered(a) != lowered(b) ? lowered(a) < lowered(b) : a < b;
        });
        const auto repeated = std::ranges::unique(names, [&](const std::string& a, const std::string& b) { return lowered(a) == lowered(b); });
        names.erase(repeated.begin(), repeated.end());
        return names;
    }

    namespace
    {
        // The shape a style is drawn as where it has no picture: the feather for all but the triangle.
        const Shape& ShapeOf(CursorStyle style) { return style == CursorStyle::Triangle ? ArrowShape() : FeatherShape(); }
    }

    int NaturalCursorWidth(CursorStyle style, int height)
    {
        return CursorIsPicture(style) ? height : ShapeWidth(ShapeOf(style), height);
    }

    CursorDrawing DrawCursor(CursorStyle style, const Image* picture, int pixelWidth, int pixelHeight, int edge,
        uint32_t color, uint32_t edgeColor)
    {
        if (picture != nullptr)
            return CursorDrawing{Edged(Resized(*picture, pixelWidth, pixelHeight), edge, edgeColor), kPictureTip};
        const Shape& shape      = ShapeOf(style);
        const Coverage coverage = ShapeCoverage(shape, pixelWidth, pixelHeight, edge);
        const float width       = static_cast<float>(coverage.width - 2 * edge);
        return CursorDrawing{Outlined(coverage, edge, color, edgeColor),
            (static_cast<float>(edge) + ShapeTip(shape) * width) / static_cast<float>(coverage.width)};
    }
}
