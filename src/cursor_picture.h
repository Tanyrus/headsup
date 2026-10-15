#pragma once

#include "cursor_file.h"
#include "plate_image.h"
#include "settings.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace headsup
{
    // PlayOnline's chocobo pointer as the target cursor shows it: its first frame.
    Image ChocoboPicture();
    // A cursor file's first frame (ReadCursorFile's); nothing for a file that gave none.
    std::optional<Image> CursorFilePicture(const std::vector<CursorFrame>& frames);
    // The kinds of file the cursors folder is read for, as their names end.
    constexpr const char* kCursorExtensions[] = {".cur", ".ani"};
    // The cursors a folder holds, from the names of everything in it: each cursor file's name without its ending,
    // once whatever its case, in the alphabet's order. A name the settings would not keep is left out.
    std::vector<std::string> CursorFileNames(const std::vector<std::string>& entries);
    // The most of a cursor file that is read: an animated cursor of many large frames is a few megabytes.
    constexpr size_t kMaxCursorFileBytes = size_t{16} << 20;

    // The width a style has of itself at a height: a shape's own, and a picture's as a square, which cursor files are.
    int NaturalCursorWidth(CursorStyle style, int height);

    struct CursorDrawing
    {
        Image image;
        float tip = 0.5f; // where it points, from 0 to 1 across the image
    };
    // The target cursor pixelWidth by pixelHeight inside an edge of edge pixels: a picture in its own colors, or
    // with none the style's shape filled with color, which for a file that was not read is the feather.
    CursorDrawing DrawCursor(CursorStyle style, const Image* picture, int pixelWidth, int pixelHeight, int edge,
        uint32_t color, uint32_t edgeColor);
}
