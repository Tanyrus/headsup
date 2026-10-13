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
    // A cursor file's first frame; nothing for bytes that are no .cur or .ani file.
    std::optional<Image> CursorFilePicture(const uint8_t* data, size_t size);
    // The kinds of file the cursors folder is read for, as their names end.
    constexpr const char* kCursorExtensions[] = {".cur", ".ani"};
    // The cursors a folder holds, from the names of everything in it: each cursor file's name without its ending,
    // once, in the alphabet's order whatever the case.
    std::vector<std::string> CursorFileNames(const std::vector<std::string>& entries);

    // The width a style has of itself at a height: a shape's own, and a picture's as a square, which cursor files are.
    int NaturalCursorWidth(CursorStyle style, int height);

    struct CursorDrawing
    {
        Image image;
        float tip = 0.5f; // where it points, from 0 to 1 across the image
    };
    // The target cursor pixelWidth by pixelHeight inside an edge of edge pixels. A shape is filled with color. A
    // picture (the chocobo, a file's) keeps its own colors and stands over the middle; a file that was not read has
    // no picture and is drawn as the feather.
    CursorDrawing DrawCursor(CursorStyle style, const Image* picture, int pixelWidth, int pixelHeight, int edge,
        uint32_t color, uint32_t edgeColor);
}
