#include "game_glyphs.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <tuple>

namespace headsup
{
    namespace
    {
        constexpr float kRunGapLetters        = 2.0f; // a gap wider than this many letter heights ends a name
        constexpr float kHidePadLetters       = 2.0f; // around a replaced name, for a stray letter
        constexpr float kOwnPadLetters        = 1.0f; // around a kept name
        constexpr float kIconReachLetters     = 8.0f; // how far left of a name the game's icons reach
        constexpr float kMaxIconHeightLetters = 4.0f;
        constexpr float kMaxIconWidthLetters  = 8.0f;
        constexpr float kMinLetterRatio       = 0.6f; // a glyph this much shorter or taller than a name's letters
        constexpr float kMaxLetterRatio       = 1.6f; // is not one of them
        // A name's letters share its cap line, and only descenders reach below its baseline, by a fifth of a letter. In a
        // fight the game also draws glowing squares and bars from the name font on name lines, sometimes credited to the
        // mob being fought; none stay inside those lines.
        constexpr float kAboveCapLine  = 0.1f; // letter heights
        constexpr float kBelowBaseline = 0.3f;

        bool SizedLikeIcons(const ScreenBox& glyph, float letterHeight)
        {
            return glyph.Height() <= kMaxIconHeightLetters * letterHeight && glyph.Width() <= kMaxIconWidthLetters * letterHeight;
        }

        // The box of count pretransformed vertices when every one is finite and its depth passes depthOk.
        template <typename DepthOk>
        bool BoxOf(const void* vertices, uint32_t stride, uint32_t count, ScreenBox& box, float& depth, DepthOk depthOk)
        {
            box   = ScreenBox{};
            depth = 0.0f;
            if (vertices == nullptr || stride < kPretransformedPositionBytes) return false;
            const auto* bytes = static_cast<const uint8_t*>(vertices);
            for (uint32_t i = 0; i < count; ++i)
            {
                float xyz[3];
                std::memcpy(xyz, bytes + static_cast<size_t>(i) * stride, sizeof(xyz));
                if (!depthOk(xyz[2]) || !std::isfinite(xyz[0]) || !std::isfinite(xyz[1])) return false;
                box.Add(xyz[0], xyz[1]);
                depth = std::max(depth, xyz[2]);
            }
            return true;
        }

        float Median(std::vector<float> values)
        {
            std::nth_element(values.begin(), values.begin() + values.size() / 2, values.end());
            return values[values.size() / 2];
        }
    }

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

    bool WorldTextBox(const void* vertices, uint32_t stride, uint32_t count, ScreenBox& box, float& depth)
    {
        if (count == 0 || count > kMaxTextVertices) return false;
        return BoxOf(vertices, stride, count, box, depth, [](float z) { return z > 0.0f && z < 1.0f; });
    }

    bool UiQuadBox(const void* vertices, uint32_t stride, ScreenBox& box)
    {
        float depth = 0.0f;
        return BoxOf(vertices, stride, kQuadVertices, box, depth, [](float z) { return z == 0.0f; });
    }

    bool LetterSized(const ScreenBox& glyph)
    {
        return glyph.valid && glyph.Width() <= kMaxGlyphSize && glyph.Height() <= kMaxGlyphSize;
    }

    ScreenBox NameplateFromGlyphs(std::vector<ScreenBox> glyphs)
    {
        glyphs.erase(std::remove_if(glyphs.begin(), glyphs.end(), [](const ScreenBox& g) { return !LetterSized(g); }), glyphs.end());
        // Left to right, each place once: the game draws some glyphs twice where they are, the first pass glowing.
        auto corners = [](const ScreenBox& g) { return std::tie(g.minX, g.minY, g.maxX, g.maxY); };
        std::sort(glyphs.begin(), glyphs.end(), [&](const ScreenBox& a, const ScreenBox& b) { return corners(a) < corners(b); });
        glyphs.erase(std::unique(glyphs.begin(), glyphs.end(), [&](const ScreenBox& a, const ScreenBox& b) { return corners(a) == corners(b); }),
            glyphs.end());
        if (glyphs.size() < kMinGlyphs) return ScreenBox{};

        // The main text line: the median glyph's vertical center and height.
        std::vector<float> centers, heights;
        for (const ScreenBox& g : glyphs)
        {
            centers.push_back(g.CenterY());
            heights.push_back(g.Height());
        }
        const float lineCenter = Median(centers);
        const float height     = std::max(Median(heights), kMinLetterHeight);
        glyphs.erase(std::remove_if(glyphs.begin(), glyphs.end(),
                         [&](const ScreenBox& g) { return std::fabs(g.CenterY() - lineCenter) > height * 0.5f; }),
            glyphs.end());
        std::vector<float> tops, bottoms;
        for (const ScreenBox& g : glyphs)
        {
            tops.push_back(g.minY);
            bottoms.push_back(g.maxY);
        }
        const float capLine = Median(tops), baseline = Median(bottoms);
        glyphs.erase(std::remove_if(glyphs.begin(), glyphs.end(),
                         [&](const ScreenBox& g) {
                             return g.minY < capLine - kAboveCapLine * height || g.maxY > baseline + kBelowBaseline * height;
                         }),
            glyphs.end());

        // Runs of glyphs left to right, broken by wide gaps; the largest run is the name.
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

    uint32_t ShownColor(uint32_t diffuse, uint32_t modulateScale)
    {
        auto channel = [&](int shift) { return static_cast<uint8_t>(std::min<uint32_t>(Channel(diffuse, shift) * modulateScale, 0xFF)); };
        return Opaque(channel(kRedShift), channel(kGreenShift), channel(kBlueShift));
    }

    uint32_t NameColor(const std::vector<GlyphDraw>& letters, const ScreenBox& plate)
    {
        std::unordered_map<uint32_t, size_t> counts;
        for (const GlyphDraw& g : letters)
            if (Inside(g.box.CenterX(), g.box.CenterY(), plate, 0.0f)) ++counts[g.argb];
        uint32_t best    = kWhite;
        size_t bestCount = 0;
        for (const auto& [argb, count] : counts)
            if (count > bestCount) best = argb, bestCount = count;
        return best;
    }

    float NameDepth(const std::vector<GlyphDraw>& letters, const ScreenBox& plate)
    {
        float depth = 0.0f;
        for (const GlyphDraw& g : letters)
            if (Inside(g.box.CenterX(), g.box.CenterY(), plate, 0.0f)) depth = std::max(depth, g.depth);
        return depth;
    }

    bool BesideName(const ScreenBox& glyph, const ScreenBox& name)
    {
        if (!glyph.valid || !name.valid) return false;
        const float h = std::max(name.Height(), kMinLetterHeight);
        if (!SizedLikeIcons(glyph, h)) return false;
        const float x = glyph.CenterX(), y = glyph.CenterY();
        return y >= name.minY - h && y <= name.maxY + h && x >= name.minX - kIconReachLetters * h && x <= name.maxX + h;
    }

    bool InNamesImage(bool letter, uintptr_t image, uintptr_t& namesImage)
    {
        if (letter) namesImage = image;
        return image == namesImage;
    }

    FrameNames ReadFrameNames(const std::unordered_map<uint16_t, std::vector<GlyphDraw>>& glyphs,
        const std::unordered_set<uint16_t>& kept, const FrameNames& last)
    {
        FrameNames names;
        std::unordered_map<uintptr_t, size_t> textures;
        for (const auto& [index, drawn] : glyphs)
            for (const GlyphDraw& g : drawn)
                ++textures[g.texture];
        names.font = last.font;
        size_t most = 0;
        for (const auto& [texture, count] : textures)
            if (count > most) names.font = texture, most = count;

        std::vector<float> heights;
        for (const auto& [index, drawn] : glyphs)
        {
            names.glyphCounts[index] = static_cast<uint32_t>(drawn.size());
            std::vector<GlyphDraw> letters;
            std::vector<ScreenBox> boxes;
            for (const GlyphDraw& g : drawn)
            {
                if (g.texture != names.font) continue;
                letters.push_back(g);
                boxes.push_back(g.box);
            }
            const ScreenBox plate = NameplateFromGlyphs(boxes);
            if (!plate.valid) continue;
            names.plates[index] = plate;
            names.colors[index] = NameColor(letters, plate);
            names.depths[index] = NameDepth(letters, plate);
            ScreenBox whole     = plate;
            for (const GlyphDraw& g : drawn)
                if (g.texture != names.font && BesideName(g.box, plate)) whole.Add(g.box);
            names.wholes[index] = whole;
            (kept.count(index) != 0 ? names.keptPlates : names.replacedPlates).push_back(plate);
            const auto run    = last.runs.find(index);
            names.runs[index] = (run == last.runs.end() ? 0 : run->second) + 1;
            std::vector<float>& recent = names.recentHeights[index];
            if (const auto before = last.recentHeights.find(index); before != last.recentHeights.end()) recent = before->second;
            recent.push_back(plate.Height());
            if (recent.size() > kSizeFrames) recent.erase(recent.begin());
            names.sizes[index] = Median(recent);
            heights.push_back(plate.Height());
        }
        names.letterHeight = heights.empty() ? last.letterHeight : Median(heights);
        return names;
    }

    bool HideReplacedGlyph(const ScreenBox& glyph, bool letter, float letterHeight, const std::vector<ScreenBox>& keptPlates)
    {
        if (!glyph.valid) return false;
        if (!letter) return SizedLikeIcons(glyph, letterHeight);
        if (!LetterSized(glyph)) return false;
        const float pad = kOwnPadLetters * std::max(glyph.Height(), kMinLetterHeight);
        for (const ScreenBox& plate : keptPlates)
            if (Inside(glyph.CenterX(), glyph.CenterY(), plate, pad)) return false;
        return true;
    }

    bool HideStrayLetter(const ScreenBox& glyph, const ScreenBox* ownPlate, const std::vector<ScreenBox>& replacedPlates)
    {
        if (!LetterSized(glyph)) return false;
        const float x      = glyph.CenterX();
        const float y      = glyph.CenterY();
        const float letter = std::max(glyph.Height(), kMinLetterHeight);
        if (ownPlate != nullptr && Inside(x, y, *ownPlate, kOwnPadLetters * letter)) return false;
        for (const ScreenBox& plate : replacedPlates)
        {
            const float ratio = letter / std::max(plate.Height(), kMinLetterHeight);
            if (ratio >= kMinLetterRatio && ratio <= kMaxLetterRatio && Inside(x, y, plate, kHidePadLetters * letter)) return true;
        }
        return false;
    }

    bool HideGameGlyph(const ScreenBox& glyph, bool letter, bool replaced, const ScreenBox* ownerName, const FrameNames& last)
    {
        if (replaced)
            return HideReplacedGlyph(glyph, letter, ownerName != nullptr ? ownerName->Height() : last.letterHeight, last.keptPlates);
        return letter && HideStrayLetter(glyph, ownerName, last.replacedPlates);
    }
}
