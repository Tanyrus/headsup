#include "nameplate.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <unordered_map>

namespace aggroglow
{
    namespace
    {
        constexpr float kRunGapLetters      = 2.0f;   // a gap wider than this many letter heights ends a name
        constexpr float kHidePadLetters     = 2.0f;   // around any mob's previous nameplate
        constexpr float kOwnerPadLetters    = 6.0f;   // around the owning mob's previous nameplate
        constexpr float kOwnPadLetters      = 1.0f;   // around a player's or NPC's own previous name
        constexpr float kMinLetterRatio     = 0.6f;   // a glyph this much shorter or taller than a name's letters
        constexpr float kMaxLetterRatio     = 1.6f;   // is not one of them
        constexpr float kNameLabelGap       = -3.0f;  // pixels: the text boxes overlap, the fonts' own spacing is enough
        constexpr float kLineGap            = 2.0f;   // pixels: above the game's name, and above the label
        constexpr float kIconGap            = 2.0f;   // pixels between icons
        constexpr float kLettersPerScreen   = 180.0f; // a typical game letter is this fraction of the screen height
        constexpr float kMinScale           = 0.5f;
        constexpr float kMaxScale           = 2.5f;
        constexpr int kSizeStep             = 2;      // pixels a font size must move before it changes

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

    bool WorldTextBox(const void* vertices, uint32_t stride, uint32_t count, ScreenBox& box)
    {
        box = ScreenBox{};
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

    bool HideGlyph(const ScreenBox& glyph, GlyphOwner owner, const ScreenBox* ownerPlate,
        const std::vector<ScreenBox>& replacedPlates)
    {
        if (!glyph.valid || glyph.Width() > kMaxGlyphSize || glyph.Height() > kMaxGlyphSize) return false;
        const float letter = std::max(glyph.Height(), 1.0f);
        const float x      = glyph.CenterX();
        const float y      = glyph.CenterY();
        if (owner == GlyphOwner::Other && ownerPlate != nullptr && Inside(x, y, *ownerPlate, kOwnPadLetters * letter))
            return false;
        auto sameLetters = [&](const ScreenBox& plate) {
            const float ratio = letter / std::max(plate.Height(), 1.0f);
            return ratio >= kMinLetterRatio && ratio <= kMaxLetterRatio;
        };
        for (const ScreenBox& plate : replacedPlates)
            if (sameLetters(plate) && Inside(x, y, plate, kHidePadLetters * letter)) return true;
        return owner == GlyphOwner::Mob && ownerPlate != nullptr && sameLetters(*ownerPlate) &&
               Inside(x, y, *ownerPlate, kOwnerPadLetters * letter);
    }

    NameplateLayout LayoutNameplate(const ScreenBox& plate, const LineSizes& sizes, bool showName)
    {
        NameplateLayout l{};
        const float centerX = plate.CenterX();
        float bottom        = plate.minY - kLineGap; // the next line up ends here
        if (showName)
        {
            l.nameX = centerX - sizes.nameWidth * 0.5f;
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
        return l;
    }

    int ScaledSize(int base, float letterHeight, float screenHeight)
    {
        float factor = screenHeight > 0.0f ? letterHeight / (screenHeight / kLettersPerScreen) : 1.0f;
        if (!std::isfinite(factor)) factor = 1.0f;
        factor = std::clamp(factor, kMinScale, kMaxScale);
        return static_cast<int>(std::lround(static_cast<float>(base) * factor));
    }

    int SteppedSize(int current, int target)
    {
        return current > 0 && std::abs(target - current) < kSizeStep ? current : target;
    }

}
