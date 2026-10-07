#include "game_glyphs.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <limits>
#include <map>
#include <tuple>

namespace headsup
{
    namespace
    {
        // Back-buffer pixels: the game draws screen-sized quads in its scene too, on some frames credited to whichever
        // entity is on the stack.
        constexpr float kMaxGlyphSize = 64.0f;
        constexpr size_t kMinGlyphs   = 3; // a three-letter name; names also have a leading quad
        constexpr size_t kSizeFrames  = 5;
        // A degenerate letter would otherwise scale its padding to nothing.
        constexpr float kMinLetterHeight      = 1.0f;
        constexpr float kRunGapLetters        = 2.0f;
        constexpr float kHidePadLetters       = 2.0f;
        constexpr float kOwnPadLetters        = 1.0f;
        constexpr float kIconReachLetters     = 8.0f;
        // The game's icon strip is 1.5 letters tall and up to 2.7 wide, larger on the sub-target.
        constexpr float kMaxIconHeightLetters = 4.0f;
        constexpr float kMaxIconWidthLetters  = 8.0f;
        constexpr float kMinLetterRatio       = 0.6f;
        constexpr float kMaxLetterRatio       = 1.6f;
        constexpr float kUntangleReach        = 3.0f; // letter heights a name can move in a frame
        // Letters share a cap line and only descenders dip below the baseline, by a fifth of a letter; the squares and
        // bars the game draws from the name font on name lines in a fight never stay inside those lines.
        constexpr float kAboveCapLine  = 0.1f;
        constexpr float kBelowBaseline = 0.3f;

        bool Inside(float x, float y, const ScreenBox& box, float pad)
        {
            return box.valid && x >= box.minX - pad && x <= box.maxX + pad && y >= box.minY - pad && y <= box.maxY + pad;
        }

        bool LetterSized(const ScreenBox& glyph)
        {
            return glyph.valid && glyph.Width() <= kMaxGlyphSize && glyph.Height() <= kMaxGlyphSize;
        }

        bool SizedLikeIcons(const ScreenBox& glyph, float letterHeight)
        {
            return glyph.Height() <= kMaxIconHeightLetters * letterHeight && glyph.Width() <= kMaxIconWidthLetters * letterHeight;
        }

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

        // The game sometimes credits other names' glyphs to whichever entity is on the stack, in a fight a whole frame's to
        // one mob. Each name's quads share a depth, so each depth's go back to the nearest name of the frame before.
        std::unordered_map<uint16_t, std::vector<GlyphDraw>> Untangle(const std::unordered_map<uint16_t, std::vector<GlyphDraw>>& glyphs,
            const FrameNames& last)
        {
            std::unordered_map<uint16_t, std::vector<GlyphDraw>> untangled;
            for (const auto& [index, drawn] : glyphs)
            {
                std::map<float, std::vector<GlyphDraw>> byDepth;
                for (const GlyphDraw& g : drawn)
                    byDepth[g.depth].push_back(g);
                for (const auto& [depth, group] : byDepth)
                {
                    uint16_t to = index;
                    if (byDepth.size() > 1)
                    {
                        ScreenBox box;
                        for (const GlyphDraw& g : group)
                            box.Add(g.box);
                        float nearest = std::numeric_limits<float>::max();
                        for (const auto& [other, plate] : last.plates)
                        {
                            const float dx = std::max({plate.minX - box.CenterX(), 0.0f, box.CenterX() - plate.maxX});
                            const float dy = std::max({plate.minY - box.CenterY(), 0.0f, box.CenterY() - plate.maxY});
                            const float distance = std::hypot(dx, dy);
                            if (distance > kUntangleReach * plate.Height()) continue;
                            if (distance < nearest || (distance == nearest && other == index)) nearest = distance, to = other;
                        }
                    }
                    std::vector<GlyphDraw>& out = untangled[to];
                    out.insert(out.end(), group.begin(), group.end());
                }
            }
            return untangled;
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

    ScreenBox NameplateFromGlyphs(std::vector<ScreenBox> glyphs)
    {
        glyphs.erase(std::remove_if(glyphs.begin(), glyphs.end(), [](const ScreenBox& g) { return !LetterSized(g); }), glyphs.end());
        // The game draws some glyphs twice in the same place, the first pass glowing.
        auto corners = [](const ScreenBox& g) { return std::tie(g.minX, g.minY, g.maxX, g.maxY); };
        std::sort(glyphs.begin(), glyphs.end(), [&](const ScreenBox& a, const ScreenBox& b) { return corners(a) < corners(b); });
        glyphs.erase(std::unique(glyphs.begin(), glyphs.end(), [&](const ScreenBox& a, const ScreenBox& b) { return corners(a) == corners(b); }),
            glyphs.end());
        if (glyphs.size() < kMinGlyphs) return ScreenBox{};

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

    FrameNames ReadFrameNames(const std::unordered_map<uint16_t, std::vector<GlyphDraw>>& credited, const TextureUse& use,
        const std::unordered_set<uint16_t>& kept, const FrameNames& last, uint16_t enlarged)
    {
        const std::unordered_map<uint16_t, std::vector<GlyphDraw>> glyphs = Untangle(credited, last);
        FrameNames names;
        names.font    = last.font;
        uint32_t most = 0;
        for (const auto& [texture, count] : use)
            if (count > most) names.font = texture, most = count;

        float largest = 0.0f;
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
            const auto inRow         = last.framesInRow.find(index);
            names.framesInRow[index] = (inRow == last.framesInRow.end() ? 0 : inRow->second) + 1;
            std::vector<float>& recent = names.recentHeights[index];
            if (const auto before = last.recentHeights.find(index); before != last.recentHeights.end()) recent = before->second;
            const bool held = index == enlarged && !recent.empty();
            if (!held) recent.push_back(plate.Height());
            if (recent.size() > kSizeFrames) recent.erase(recent.begin());
            names.sizes[index] = Median(recent);
            // The game grows a picked name downward from its top edge: the name sits where its normal size would.
            if (held) names.plates[index].maxY = names.plates[index].minY + names.sizes[index];
            largest = std::max(largest, plate.Height());
        }
        names.largestLetterHeight = names.plates.empty() ? last.largestLetterHeight : largest;
        for (const auto& [index, seen] : last.lastSeen)
            if (seen.framesGone < kRememberedNameFrames) names.lastSeen[index] = {seen.height, seen.framesGone + 1};
        for (const auto& [index, plate] : names.plates)
            names.lastSeen[index] = {plate.Height(), 0};
        return names;
    }

    bool HideReplacedGlyph(const ScreenBox& glyph, bool letter, float letterHeight, const std::vector<ScreenBox>& keptPlates)
    {
        if (!glyph.valid) return false;
        if (!letter || !LetterSized(glyph)) return SizedLikeIcons(glyph, letterHeight);
        const float pad = kOwnPadLetters * std::max(glyph.Height(), kMinLetterHeight);
        for (const ScreenBox& plate : keptPlates)
            if (Inside(glyph.CenterX(), glyph.CenterY(), plate, pad)) return false;
        return true;
    }

    bool HideStrayLetter(const ScreenBox& glyph, const ScreenBox* ownPlate, const std::vector<ScreenBox>& replacedPlates)
    {
        if (!LetterSized(glyph)) return false;
        const float x            = glyph.CenterX();
        const float y            = glyph.CenterY();
        const float letterHeight = std::max(glyph.Height(), kMinLetterHeight);
        if (ownPlate != nullptr && Inside(x, y, *ownPlate, kOwnPadLetters * letterHeight)) return false;
        for (const ScreenBox& plate : replacedPlates)
        {
            const float ratio = letterHeight / std::max(plate.Height(), kMinLetterHeight);
            if (ratio >= kMinLetterRatio && ratio <= kMaxLetterRatio && Inside(x, y, plate, kHidePadLetters * letterHeight)) return true;
        }
        return false;
    }

    float LastLetterHeight(const FrameNames& last, uint16_t index)
    {
        const auto seen = last.lastSeen.find(index);
        return seen != last.lastSeen.end() ? seen->second.height : 0.0f;
    }

    bool HideGameGlyph(const ScreenBox& glyph, bool letter, bool replaced, const ScreenBox* ownerName, float ownerHeight,
        const FrameNames& last)
    {
        if (replaced)
            return HideReplacedGlyph(glyph, letter, std::max(ownerHeight, last.largestLetterHeight), last.keptPlates);
        return letter && HideStrayLetter(glyph, ownerName, last.replacedPlates);
    }
}
