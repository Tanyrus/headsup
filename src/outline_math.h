#pragma once

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

    void OutlineOffset(int tap, int taps, float px, float width, float height, float& dxNdc, float& dyNdc);

    // An outline's thickness is a percent of this many render-target pixels.
    constexpr float kOutlineUnitPx = 4.0f;
    float OutlinePixels(int percent);

    // How round an outline is: the shifted copies of each mesh that draw it.
    enum class SmoothnessLevel { Low, Medium, High, Ultra };
    constexpr int kSmoothnessLevelCount = 4;
    constexpr int kCopiesPerLevel       = 4;
    constexpr int SmoothnessCopies(SmoothnessLevel level) { return kCopiesPerLevel * (static_cast<int>(level) + 1); }
    // The level nearest a count of copies, the higher of two as near.
    SmoothnessLevel SmoothnessLevelOf(int copies);

    constexpr float kFadeNearYalms = 10.0f; // what fades in is whole from here in
    constexpr float kFadeFarShare  = 0.01f; // and this share of itself at its range and beyond
    // The share of its opacity that an outline or a nameplate fading in has at a distance, even between the two.
    constexpr float FadeShare(float distance, float range)
    {
        if (distance <= kFadeNearYalms) return 1.0f;
        if (distance >= range) return kFadeFarShare;
        return 1.0f + (kFadeFarShare - 1.0f) * (distance - kFadeNearYalms) / (range - kFadeNearYalms);
    }
    // An outline's alpha out of 255, from its opacity in percent.
    uint8_t OutlineAlpha(int opacityPercent, float distance, float range, bool fadeIn);
    // An alpha test's reference for a texture whose alpha is multiplied by alpha out of 255: a texel then passes
    // where it passed before, so a mesh keeps its cut-outs however faint it is drawn.
    uint32_t ScaledAlphaRef(uint32_t reference, uint8_t alpha);
}
