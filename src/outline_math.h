#pragma once

#include "tracker.h"

#include <cstdint>

namespace aggroglow
{
    struct Mat4
    {
        float m[16]; // row-major, the same layout as D3DMATRIX (_11 ... _44)
    };

    bool IsIdentity(const Mat4& m, float epsilon = 1e-4f);

    // A projection whose clip-space x/y move by (dxNdc, dyNdc) * w: the same screen offset at every depth.
    Mat4 ShiftProjection(const Mat4& p, float dxNdc, float dyNdc);

    // NDC offset for direction `tap` of `taps` evenly spaced directions, `px` pixels from the centre of a
    // width x height viewport.
    void OutlineOffset(int tap, int taps, float px, float width, float height, float& dxNdc, float& dyNdc);

    // The draw owner: the first word in [begin, end) that is a tracked actor pointer. That is the live actor being
    // drawn; pointers further up the stack can be stale leftovers. nullptr when none is found.
    const ActorInfo* FindOwner(const uint32_t* begin, const uint32_t* end, const Tracker& tracker);

    // D3DFMT_D15S1 (73), D3DFMT_D24S8 (75) and D3DFMT_D24X4S4 (79) have stencil bits.
    bool HasStencilBits(uint32_t depthFormat);

    // Whether the bound depth surface has stencil bits, re-reading its format only when the surface changes.
    class StencilCheck
    {
    public:
        template <typename FormatOf>
        bool HasStencil(uintptr_t surface, FormatOf formatOf)
        {
            if (surface != m_Surface)
            {
                m_Surface    = surface;
                m_HasStencil = HasStencilBits(formatOf());
            }
            return m_HasStencil;
        }

    private:
        uintptr_t m_Surface = 0;
        bool m_HasStencil   = false;
    };
}
