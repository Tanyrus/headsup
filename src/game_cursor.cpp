#include "game_cursor.h"

#include "argb.h"

#include <algorithm>
#include <cmath>

namespace headsup
{
    namespace
    {
        // Back-buffer pixels: the game's cursor is about 27x43 on 1440p.
        constexpr float kMinCursorSide  = 4.0f;
        constexpr float kMaxCursorSide  = 128.0f;
        constexpr float kCursorReach    = 0.5f;
        constexpr float kAnchorSlack    = 2.0f;  // UI pixels
        constexpr float kMinArrowHeight = 16.0f; // UI pixels: the arrows are 32 tall, the window's letters at most 12

        bool CursorSized(const ScreenBox& quad)
        {
            const float w = quad.Width(), h = quad.Height();
            return quad.valid && w >= kMinCursorSide && h >= kMinCursorSide && w <= kMaxCursorSide && h <= kMaxCursorSide;
        }

        bool InArrowTexture(const CursorQuad& quad, uintptr_t arrowTexture)
        {
            return arrowTexture != 0 && quad.texture == arrowTexture && LooksLikeTargetArrow(quad.ui);
        }
    }

    std::optional<bool> PickedOutOfRange(uint32_t argb)
    {
        const uint8_t red = Channel(argb, kRedShift), blue = Channel(argb, kBlueShift);
        if (red == blue) return std::nullopt;
        return red > blue;
    }

    CursorTargets TargetsFromSlots(bool picking, uint32_t slot0, uint32_t slot1, bool locked, uint32_t entityCount)
    {
        auto index = [&](uint32_t slot) { return slot < entityCount ? static_cast<uint16_t>(slot) : uint16_t{0}; };
        return picking ? CursorTargets{index(slot1), index(slot0), locked} : CursorTargets{index(slot0), 0, locked};
    }

    void PlaceAnchors(CursorTargets& targets, const CursorWindow& window, float menuWidth, float menuHeight,
        float backBufferWidth, float backBufferHeight)
    {
        if (menuWidth <= 0.0f || menuHeight <= 0.0f || backBufferWidth <= 0.0f || backBufferHeight <= 0.0f) return;
        const float x = backBufferWidth / menuWidth, y = backBufferHeight / menuHeight;
        targets.anchored   = true;
        targets.anchorX    = window.ankX * x;
        targets.anchorY    = window.ankY * y;
        targets.subAnchorX = window.subAnkX * x;
        targets.subAnchorY = window.subAnkY * y;
    }

    std::vector<CursorAnchor> GameCursorAnchors(const std::vector<CursorName>& names, const CursorWindow& window, bool picking)
    {
        std::vector<CursorAnchor> anchors;
        for (const CursorName& n : names)
        {
            anchors.push_back({n.index, window.ankX, window.ankY});
            if (picking) anchors.push_back({n.index, window.subAnkX, window.subAnkY});
        }
        return anchors;
    }

    bool AtCursorAnchor(const ScreenBox& quad, float anchorX, float anchorY)
    {
        return quad.valid && std::fabs(quad.CenterX() - anchorX) <= kAnchorSlack && quad.maxY <= anchorY + kAnchorSlack &&
               quad.maxY >= anchorY - quad.Height();
    }

    bool LooksLikeTargetArrow(const ScreenBox& quad) { return quad.valid && quad.Height() > quad.Width(); }

    bool IsGameCursor(const ScreenBox& quad, const ScreenBox& name)
    {
        if (!name.valid || !CursorSized(quad)) return false;
        if (std::fabs(quad.CenterX() - name.CenterX()) > quad.Width() * 0.5f) return false;
        return quad.maxY <= name.maxY && quad.maxY >= name.minY - quad.Height() * kCursorReach;
    }

    bool MayBeGameCursor(const CursorQuad& quad, uintptr_t arrowTexture, const std::vector<CursorAnchor>& anchors,
        const std::vector<CursorName>& names)
    {
        if (names.empty()) return false;
        return InArrowTexture(quad, arrowTexture) ||
               std::any_of(anchors.begin(), anchors.end(), [&](const CursorAnchor& a) { return AtCursorAnchor(quad.ui, a.x, a.y); }) ||
               std::any_of(names.begin(), names.end(), [&](const CursorName& n) { return IsGameCursor(quad.screen, n.name); });
    }

    CursorVerdict JudgeGameCursor(const CursorQuad& quad, uintptr_t arrowTexture, uintptr_t fontTexture,
        const std::vector<CursorAnchor>& anchors, const std::vector<CursorName>& names, std::optional<uint16_t> owner)
    {
        CursorVerdict verdict;
        if (names.empty()) return verdict;
        if (InArrowTexture(quad, arrowTexture))
        {
            verdict.block = true;
            return verdict;
        }
        const bool anchored =
            std::any_of(anchors.begin(), anchors.end(), [&](const CursorAnchor& a) { return AtCursorAnchor(quad.ui, a.x, a.y); });
        const bool credited =
            owner.has_value() &&
            (std::any_of(anchors.begin(), anchors.end(),
                 [&](const CursorAnchor& a) { return a.index == *owner && AtCursorAnchor(quad.ui, a.x, a.y); }) ||
                std::any_of(names.begin(), names.end(),
                    [&](const CursorName& n) { return n.index == *owner && IsGameCursor(quad.screen, n.name); }));
        verdict.block      = anchored || credited;
        verdict.learnArrow = credited && quad.texture != 0 && quad.texture != fontTexture && LooksLikeTargetArrow(quad.ui) &&
                             quad.ui.Height() >= kMinArrowHeight && CursorSized(quad.screen);
        return verdict;
    }
}
