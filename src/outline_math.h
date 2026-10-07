#pragma once

#include "tracker.h"

#include <cstdint>

namespace headsup
{
    struct Mat4
    {
        float m[16]; // row-major, the same layout as D3DMATRIX (_11 ... _44)
    };

    bool IsIdentity(const Mat4& m);

    // A projection whose clip-space x/y move by (dxNdc, dyNdc) * w: the same screen offset at every depth.
    Mat4 ShiftProjection(const Mat4& p, float dxNdc, float dyNdc);

    // NDC offset for direction `tap` of `taps` evenly spaced directions, `px` pixels from the centre of a
    // width x height viewport (both above 0).
    void OutlineOffset(int tap, int taps, float px, float width, float height, float& dxNdc, float& dyNdc);

    // The draw owner: the first word in [begin, end) that is a tracked actor pointer. That is the live actor being
    // drawn; pointers further up the stack can be stale leftovers. nullptr when none is found.
    const ActorInfo* FindOwner(const uint32_t* begin, const uint32_t* end, const Tracker& tracker);

    // The D3DFORMAT depth formats with stencil bits (this file is built without the Direct3D headers).
    constexpr uint32_t kFormatD15S1 = 73, kFormatD24S8 = 75, kFormatD24X4S4 = 79;
    bool HasStencilBits(uint32_t depthFormat);
}
