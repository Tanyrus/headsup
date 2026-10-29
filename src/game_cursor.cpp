#include "game_cursor.h"

#include "argb.h"
#include "mouse_messages.h"

#include <algorithm>
#include <iterator>
#include <utility>


namespace headsup
{
    std::optional<bool> PickedOutOfRange(uint32_t argb)
    {
        const uint8_t red = Channel(argb, kRedShift), blue = Channel(argb, kBlueShift);
        if (red == blue) return std::nullopt;
        return red > blue;
    }

    bool Picking(bool subTargetActive, bool windowPicking, uint32_t slot0)
    {
        return subTargetActive || (windowPicking && slot0 != 0);
    }

    uint16_t SlotTarget(uint32_t index, bool active, bool model, uint32_t entityCount)
    {
        if (index == 0) return active && model ? kModelTarget : uint16_t{0};
        return index < entityCount ? static_cast<uint16_t>(index) : uint16_t{0};
    }

    CursorTargets TargetsFromSlots(bool picking, uint16_t slot0, uint16_t slot1, bool locked)
    {
        return picking ? CursorTargets{slot1, slot0, locked} : CursorTargets{slot0, 0, locked};
    }

    MenuSize MenuSizeOf(float registryWidth, float registryHeight, float backBufferWidth, float backBufferHeight)
    {
        return MenuSize{registryWidth > 0.0f ? registryWidth : backBufferWidth,
            registryHeight > 0.0f ? registryHeight : backBufferHeight};
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

    bool GamePresses::Keep(uint32_t message, bool kept)
    {
        const auto press = [&](bool& got) {
            got = !kept;
            return kept;
        };
        const auto release = [&](bool& got) { return !std::exchange(got, false); };
        switch (message)
        {
        case kLeftDown: return press(m_Left);
        case kRightDown: return press(m_Right);
        case kMiddleDown: return press(m_Middle);
        case kLeftUp: return release(m_Left);
        case kRightUp: return release(m_Right);
        case kMiddleUp: return release(m_Middle);
        default: return kept;
        }
    }

    bool ClickMessage(uint32_t message)
    {
        constexpr uint32_t kClicks[] = {kLeftDown, kLeftDouble, kRightDown, kRightDouble, kMiddleDown, kMiddleDouble,
            kXButtonDown, kXButtonDouble};
        return std::find(std::begin(kClicks), std::end(kClicks), message) != std::end(kClicks);
    }
}
