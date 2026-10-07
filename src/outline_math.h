#pragma once

namespace headsup
{
    struct Mat4
    {
        float m[16]; // row-major, the same layout as D3DMATRIX (_11 ... _44)
    };

    bool IsIdentity(const Mat4& m);

    // A projection whose clip-space x/y move by (dxNdc, dyNdc) * w: the same screen offset at every depth.
    Mat4 ShiftProjection(const Mat4& p, float dxNdc, float dyNdc);

    void OutlineOffset(int tap, int taps, float px, float width, float height, float& dxNdc, float& dyNdc);
}
