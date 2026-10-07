#include "nameplate.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <unordered_map>

namespace headsup
{
    namespace
    {
        constexpr float kRunGapLetters      = 2.0f;   // a gap wider than this many letter heights ends a name
        constexpr float kHidePadLetters     = 2.0f;   // around any mob's previous nameplate
        constexpr float kOwnerPadLetters    = 6.0f;   // around the owning mob's previous nameplate
        constexpr float kOwnPadLetters      = 1.0f;   // around a kept name, the owner's own
        constexpr float kIconReachLetters   = 8.0f;   // how far left of a name the game's icons reach
        constexpr float kMaxIconHeightLetters = 2.0f; // the game's icon strip: 1.5 letters tall, up to 2.7 wide
        constexpr float kMaxIconWidthLetters  = 4.0f;
        constexpr float kMinLetterRatio     = 0.6f;   // a glyph this much shorter or taller than a name's letters
        constexpr float kMaxLetterRatio     = 1.6f;   // is not one of them
        constexpr float kNameLabelGap       = -3.0f;  // pixels: the text boxes overlap, the fonts' own spacing is enough
        constexpr float kLineGap            = 2.0f;   // pixels: above the game's name, and above the label
        constexpr float kIconGap            = 2.0f;   // pixels between icons
        constexpr float kLettersPerScreen   = 180.0f; // a typical game letter is this fraction of the screen height
        constexpr float kMinScale           = 0.5f;
        constexpr float kMaxScale           = 2.5f;
        constexpr float kMinCursorSide      = 4.0f;   // pixels: the game's target cursor is about 27x43 on 1440p
        constexpr float kMaxCursorSide      = 128.0f;
        constexpr float kCursorReach        = 0.5f;   // of its height: how far above the name its bottom may be
        constexpr double kBobPeriod         = 1.2;    // seconds
        constexpr float kBobShare           = 0.15f;  // of the cursor's height
        constexpr float kRasterSmallest     = 6.0f;   // pixels
        constexpr float kRasterStep         = 1.25f;
        constexpr float kRasterOverscale    = 1.05f;  // text is scaled up this much before it is redrawn larger

        bool Inside(float x, float y, const ScreenBox& box, float pad)
        {
            return box.valid && x >= box.minX - pad && x <= box.maxX + pad && y >= box.minY - pad && y <= box.maxY + pad;
        }
    }

    void ScreenBox::Add(float x, float y)
    {
        if (!valid)
        {
            minX = maxX = x;
            minY = maxY = y;
            valid       = true;
            return;
        }
        minX = std::min(minX, x);
        minY = std::min(minY, y);
        maxX = std::max(maxX, x);
        maxY = std::max(maxY, y);
    }

    void ScreenBox::Add(const ScreenBox& other)
    {
        if (!other.valid) return;
        Add(other.minX, other.minY);
        Add(other.maxX, other.maxY);
    }

    ScreenBox ScreenBox::Scaled(float scaleX, float scaleY) const
    {
        ScreenBox r = *this;
        r.minX *= scaleX;
        r.maxX *= scaleX;
        r.minY *= scaleY;
        r.maxY *= scaleY;
        return r;
    }

    uint32_t VertexCount(uint32_t primitiveType, uint32_t primitiveCount)
    {
        switch (primitiveType)
        {
            case 1: return primitiveCount;     // point list
            case 2: return primitiveCount * 2; // line list
            case 3: return primitiveCount + 1; // line strip
            case 4: return primitiveCount * 3; // triangle list
            case 5:                            // triangle strip
            case 6: return primitiveCount + 2; // triangle fan
            default: return 0;
        }
    }

    bool WorldTextBox(const void* vertices, uint32_t stride, uint32_t count, ScreenBox& box, float& depth)
    {
        box   = ScreenBox{};
        depth = 0.0f;
        if (vertices == nullptr || stride < kPretransformedPositionBytes || count == 0 || count > kMaxTextVertices) return false;
        const auto* bytes = static_cast<const uint8_t*>(vertices);
        for (uint32_t i = 0; i < count; ++i)
        {
            float xyz[3];
            std::memcpy(xyz, bytes + static_cast<size_t>(i) * stride, sizeof(xyz));
            if (!(xyz[2] > 0.0f && xyz[2] < 1.0f) || !std::isfinite(xyz[0]) || !std::isfinite(xyz[1]))
            {
                box = ScreenBox{};
                return false;
            }
            box.Add(xyz[0], xyz[1]);
            depth = std::max(depth, xyz[2]);
        }
        return true;
    }

    ScreenBox NameplateFromGlyphs(std::vector<ScreenBox> glyphs)
    {
        glyphs.erase(std::remove_if(glyphs.begin(), glyphs.end(),
                         [](const ScreenBox& g) {
                             return !g.valid || g.Width() > kMaxGlyphSize || g.Height() > kMaxGlyphSize;
                         }),
            glyphs.end());
        if (glyphs.size() < kMinGlyphs) return ScreenBox{};

        // The main text line: the median glyph's vertical center and height.
        std::vector<float> centers, heights;
        for (const ScreenBox& g : glyphs)
        {
            centers.push_back(g.CenterY());
            heights.push_back(g.Height());
        }
        std::nth_element(centers.begin(), centers.begin() + centers.size() / 2, centers.end());
        std::nth_element(heights.begin(), heights.begin() + heights.size() / 2, heights.end());
        const float lineCenter = centers[centers.size() / 2];
        const float height     = std::max(heights[heights.size() / 2], 1.0f);
        glyphs.erase(std::remove_if(glyphs.begin(), glyphs.end(),
                         [&](const ScreenBox& g) { return std::fabs(g.CenterY() - lineCenter) > height * 0.5f; }),
            glyphs.end());

        // Runs of glyphs left to right, broken by wide gaps; the largest run is the name.
        std::sort(glyphs.begin(), glyphs.end(), [](const ScreenBox& a, const ScreenBox& b) { return a.minX < b.minX; });
        ScreenBox best, run;
        size_t bestCount = 0, runCount = 0;
        for (const ScreenBox& g : glyphs)
        {
            if (run.valid && g.minX - run.maxX > kRunGapLetters * height)
            {
                if (runCount > bestCount) best = run, bestCount = runCount;
                run      = ScreenBox{};
                runCount = 0;
            }
            run.Add(g);
            ++runCount;
        }
        if (runCount > bestCount) best = run, bestCount = runCount;
        return bestCount >= kMinGlyphs ? best : ScreenBox{};
    }

    bool LabelVisible(const ScreenBox* plate, uint32_t meshDraws, uint32_t plateFramesInRow, float screenWidth, float screenHeight)
    {
        if (plate == nullptr || !plate->valid || meshDraws == 0 || plateFramesInRow < kStableFrames) return false;
        return plate->maxX > 0.0f && plate->maxY > 0.0f && plate->minX < screenWidth && plate->minY < screenHeight;
    }

    uint32_t ShownColor(uint32_t diffuse, uint32_t modulateScale)
    {
        uint32_t argb = 0xFF000000;
        for (const uint32_t shift : {16u, 8u, 0u})
            argb |= std::min<uint32_t>(((diffuse >> shift) & 0xFF) * modulateScale, 0xFF) << shift;
        return argb;
    }

    uint32_t NameColor(const std::vector<GlyphDraw>& glyphs, const ScreenBox& plate)
    {
        std::unordered_map<uint32_t, size_t> counts;
        for (const GlyphDraw& g : glyphs)
            if (Inside(g.box.CenterX(), g.box.CenterY(), plate, 0.0f)) ++counts[g.argb];
        uint32_t best    = kWhite;
        size_t bestCount = 0;
        for (const auto& [argb, count] : counts)
            if (count > bestCount) best = argb, bestCount = count;
        return best;
    }

    bool HideGlyph(const ScreenBox& glyph, bool isLetter, GlyphOwner owner, const ScreenBox* ownerPlate,
        const std::vector<ScreenBox>& replacedPlates)
    {
        if (!glyph.valid) return false;
        const float x = glyph.CenterX();
        const float y = glyph.CenterY();
        if (!isLetter) return owner == GlyphOwner::Replaced && ownerPlate != nullptr && BesideName(glyph, *ownerPlate);
        if (glyph.Width() > kMaxGlyphSize || glyph.Height() > kMaxGlyphSize) return false;
        const float letter = std::max(glyph.Height(), 1.0f);
        if (owner == GlyphOwner::Kept && ownerPlate != nullptr && Inside(x, y, *ownerPlate, kOwnPadLetters * letter))
            return false;
        auto sameLetters = [&](const ScreenBox& plate) {
            const float ratio = letter / std::max(plate.Height(), 1.0f);
            return ratio >= kMinLetterRatio && ratio <= kMaxLetterRatio;
        };
        for (const ScreenBox& plate : replacedPlates)
            if (sameLetters(plate) && Inside(x, y, plate, kHidePadLetters * letter)) return true;
        return owner == GlyphOwner::Replaced && ownerPlate != nullptr && sameLetters(*ownerPlate) &&
               Inside(x, y, *ownerPlate, kOwnerPadLetters * letter);
    }

    NameplateLayout LayoutNameplate(const ScreenBox& plate, const LineSizes& sizes, bool showName)
    {
        NameplateLayout l{};
        const float centerX = plate.CenterX();
        float bottom        = plate.minY - kLineGap; // the next line up ends here
        const float nameIconsWidth =
            sizes.nameIconCount > 0 ? sizes.nameIconCount * sizes.nameIconSize + (sizes.nameIconCount - 1) * kIconGap : 0.0f;
        if (showName)
        {
            const float shift = sizes.centerNameAndIcons && sizes.nameIconCount > 0 ? (nameIconsWidth + kIconGap) * 0.5f : 0.0f;
            l.nameX           = centerX - sizes.nameWidth * 0.5f + shift;
            l.nameY = plate.CenterY() - sizes.nameHeight * 0.5f;
            bottom  = l.nameY - kNameLabelGap;
        }
        if (sizes.labelHeight > 0.0f)
        {
            l.labelX = centerX - sizes.labelWidth * 0.5f;
            l.labelY = bottom - sizes.labelHeight;
            bottom   = l.labelY - kLineGap;
        }
        const float row = sizes.iconCount > 0 ? sizes.iconCount * sizes.iconSize + (sizes.iconCount - 1) * kIconGap : 0.0f;
        l.iconsX        = centerX - row * 0.5f;
        l.iconsY        = bottom - sizes.iconSize;
        l.iconStep      = sizes.iconSize + kIconGap;
        if (sizes.iconCount > 0) bottom = l.iconsY - kLineGap;
        l.nameIconStep = sizes.nameIconSize + kIconGap;
        l.nameIconsX   = l.nameX - kIconGap - nameIconsWidth;
        l.nameIconsY   = l.nameY + (sizes.nameHeight - sizes.nameIconSize) * 0.5f;
        l.cursorX = centerX - sizes.cursorWidth * sizes.cursorTip;
        l.cursorY = bottom - sizes.cursorHeight;
        return l;
    }

    ScreenBox CenteredOver(const ScreenBox& letters, const ScreenBox& whole)
    {
        ScreenBox moved = letters;
        const float dx  = whole.CenterX() - letters.CenterX();
        moved.minX += dx;
        moved.maxX += dx;
        return moved;
    }

    bool BesideName(const ScreenBox& glyph, const ScreenBox& name)
    {
        if (!glyph.valid || !name.valid) return false;
        // Sized like the name: a close player's icon strip is wider than any letter.
        const float h = std::max(name.Height(), 1.0f);
        if (glyph.Height() > kMaxIconHeightLetters * h || glyph.Width() > kMaxIconWidthLetters * h) return false;
        const float x = glyph.CenterX(), y = glyph.CenterY();
        return y >= name.minY - h && y <= name.maxY + h && x >= name.minX - kIconReachLetters * h && x <= name.maxX + h;
    }

    bool UiQuadBox(const void* vertices, uint32_t stride, ScreenBox& box)
    {
        box = ScreenBox{};
        if (vertices == nullptr || stride < kPretransformedPositionBytes) return false;
        const auto* bytes = static_cast<const uint8_t*>(vertices);
        for (uint32_t i = 0; i < kQuadVertices; ++i)
        {
            float xyz[3];
            std::memcpy(xyz, bytes + static_cast<size_t>(i) * stride, sizeof(xyz));
            if (xyz[2] != 0.0f || !std::isfinite(xyz[0]) || !std::isfinite(xyz[1]))
            {
                box = ScreenBox{};
                return false;
            }
            box.Add(xyz[0], xyz[1]);
        }
        return true;
    }

    bool IsGameCursor(const ScreenBox& quad, const ScreenBox& name)
    {
        if (!quad.valid || !name.valid) return false;
        const float w = quad.Width(), h = quad.Height();
        if (w < kMinCursorSide || h < kMinCursorSide || w > kMaxCursorSide || h > kMaxCursorSide) return false;
        if (std::fabs(quad.CenterX() - name.CenterX()) > w * 0.5f) return false;
        return quad.maxY <= name.maxY && quad.maxY >= name.minY - h * kCursorReach;
    }

    float CursorBob(double seconds, float height)
    {
        constexpr double kTwoPi = 6.283185307179586;
        const double phase      = std::sin(kTwoPi * seconds / kBobPeriod);
        return -static_cast<float>((phase + 1.0) * 0.5) * height * kBobShare;
    }

    float DistanceScale(float letterHeight, float screenHeight)
    {
        const float factor = screenHeight > 0.0f ? letterHeight / (screenHeight / kLettersPerScreen) : 1.0f;
        return std::isfinite(factor) ? std::clamp(factor, kMinScale, kMaxScale) : 1.0f;
    }

    int RasterHeight(float pixels, int current)
    {
        const auto drawn = static_cast<float>(current);
        if (current > 0 && pixels <= drawn * kRasterOverscale && pixels * kRasterStep >= drawn) return current;
        float size = kRasterSmallest;
        while (size < pixels)
            size *= kRasterStep;
        return static_cast<int>(std::lround(size));
    }

}
