#include "game_glyphs.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>

namespace headsup
{
    uint32_t VertexCount(uint32_t primitiveType, uint32_t primitiveCount)
    {
        switch (primitiveType)
        {
            case kPointList: return primitiveCount;
            case kLineList: return primitiveCount * 2;
            case kLineStrip: return primitiveCount + 1;
            case kTriangleList: return primitiveCount * 3;
            case kTriangleStrip:
            case kTriangleFan: return primitiveCount + 2;
            default: return 0;
        }
    }
}
