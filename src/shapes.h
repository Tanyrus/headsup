#pragma once

#include "text_image.h"

#include <cstddef>

namespace headsup
{
    // A point of a closed outline in the unit box, x to the right and y down.
    struct ShapePoint
    {
        float x, y;
    };

    struct Shape
    {
        const ShapePoint* points;
        size_t count;
        float aspect; // height over width
    };

    // The target cursor's shapes: a downward arrow, and Phoenix's feather icon (third_party/phoenix-feather).
    const Shape& ArrowShape();
    const Shape& FeatherShape();

    // Where the shape points: the x, from 0 to 1 across its width, of its lowest point.
    float ShapeTip(const Shape& shape);

    // The shape filled pixelHeight tall and as wide as its aspect makes it, antialiased, inside a margin of empty pixels
    // for its outline.
    Coverage ShapeCoverage(const Shape& shape, int pixelHeight, int margin);
}
