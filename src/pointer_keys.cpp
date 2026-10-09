#include "pointer_keys.h"

#include <algorithm>
#include <cstdio>

namespace headsup
{
    namespace
    {
        constexpr uint8_t kShift = 0x10, kCtrl = 0x11, kAlt = 0x12;
        constexpr uint8_t kLeftShift = 0xA0, kRightShift = 0xA1, kLeftCtrl = 0xA2, kRightCtrl = 0xA3, kLeftAlt = 0xA4, kRightAlt = 0xA5;
        constexpr uint8_t kMouseButtons[] = {0x01, 0x02, 0x04, 0x05, 0x06}; // left, right, middle, X1, X2
        constexpr uint8_t kDigit0 = 0x30, kDigit9 = 0x39, kLetterA = 0x41, kLetterZ = 0x5A;
        constexpr uint8_t kNumpad0 = 0x60, kNumpad9 = 0x69, kF1 = 0x70, kF24 = 0x87;

        struct Named
        {
            uint8_t vk;
            const char* name;
        };
        constexpr Named kNames[] = {{0x08, "Backspace"}, {0x09, "Tab"}, {0x0D, "Enter"}, {kShift, "Shift"}, {kCtrl, "Ctrl"},
            {kAlt, "Alt"}, {0x13, "Pause"}, {0x14, "Caps Lock"}, {0x1B, "Esc"}, {0x20, "Space"}, {0x21, "Page Up"},
            {0x22, "Page Down"}, {0x23, "End"}, {0x24, "Home"}, {0x25, "Left"}, {0x26, "Up"}, {0x27, "Right"}, {0x28, "Down"},
            {0x2D, "Insert"}, {0x2E, "Delete"}, {0x6A, "Num *"}, {0x6B, "Num +"}, {0x6D, "Num -"}, {0x6E, "Num ."},
            {0x6F, "Num /"}, {0x90, "Num Lock"}, {0x91, "Scroll Lock"}};
    }

    uint8_t NormalizeKey(uint8_t vk)
    {
        switch (vk)
        {
        case kLeftShift:
        case kRightShift: return kShift;
        case kLeftCtrl:
        case kRightCtrl: return kCtrl;
        case kLeftAlt:
        case kRightAlt: return kAlt;
        default: return vk;
        }
    }

    bool MouseButton(uint8_t vk) { return std::ranges::find(kMouseButtons, vk) != std::end(kMouseButtons); }

    bool KeepsPointer(const std::vector<uint8_t>& held, const std::vector<uint8_t>& keep)
    {
        return !held.empty() && std::ranges::all_of(held, [&](uint8_t vk) { return std::ranges::find(keep, vk) != keep.end(); });
    }

    std::string KeyName(uint8_t vk)
    {
        char name[16];
        if ((vk >= kDigit0 && vk <= kDigit9) || (vk >= kLetterA && vk <= kLetterZ)) return std::string(1, static_cast<char>(vk));
        if (vk >= kNumpad0 && vk <= kNumpad9) std::snprintf(name, sizeof(name), "Num %d", vk - kNumpad0);
        else if (vk >= kF1 && vk <= kF24) std::snprintf(name, sizeof(name), "F%d", vk - kF1 + 1);
        else if (const auto known = std::ranges::find(kNames, vk, &Named::vk); known != std::end(kNames)) return known->name;
        else std::snprintf(name, sizeof(name), "Key %02X", vk);
        return name;
    }
}
