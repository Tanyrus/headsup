#include "game_cursor.h"

#include "argb.h"

#include <algorithm>
#include <iterator>


namespace headsup
{
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

    bool ClickMessage(uint32_t message)
    {
        constexpr uint32_t kClicks[] = {0x201, 0x203, 0x204, 0x206, 0x207, 0x209, 0x20B, 0x20D}; // each button's down, double
        return std::find(std::begin(kClicks), std::end(kClicks), message) != std::end(kClicks);
    }
}
