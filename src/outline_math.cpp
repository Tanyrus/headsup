#include "outline_math.h"

#include <cmath>
#include <numbers>

namespace headsup
{
    bool IsIdentity(const Mat4& m)
    {
        constexpr float kTolerance = 1e-4f; // a skinned character's world matrix is identity to float precision
        for (int i = 0; i < 16; ++i)
            if (std::fabs(m.m[i] - (i % 5 == 0 ? 1.0f : 0.0f)) > kTolerance) return false;
        return true;
    }

    Mat4 ShiftProjection(const Mat4& p, float dxNdc, float dyNdc)
    {
        // Row-vector convention (clip = v * P): column 0 (x) gains dx times column 3 (w), column 1 (y) gains dy times it.
        Mat4 r = p;
        for (int row = 0; row < 4; ++row)
        {
            r.m[row * 4 + 0] += dxNdc * p.m[row * 4 + 3];
            r.m[row * 4 + 1] += dyNdc * p.m[row * 4 + 3];
        }
        return r;
    }

    void OutlineOffset(int tap, int taps, float px, float width, float height, float& dxNdc, float& dyNdc)
    {
        const float angle = 2.0f * std::numbers::pi_v<float> * static_cast<float>(tap) / static_cast<float>(taps);
        dxNdc = 2.0f * px * std::cos(angle) / width;
        dyNdc = 2.0f * px * std::sin(angle) / height;
    }

    const ActorInfo* FindOwner(const uint32_t* begin, const uint32_t* end, const Tracker& tracker)
    {
        for (const uint32_t* p = begin; p < end; ++p)
            if (const ActorInfo* info = tracker.Find(*p)) return info;
        return nullptr;
    }

    bool HasStencilBits(uint32_t depthFormat)
    {
        return depthFormat == kFormatD15S1 || depthFormat == kFormatD24S8 || depthFormat == kFormatD24X4S4;
    }
}
