#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace headsup
{
    // Keys are Windows virtual-key codes, and left and right Ctrl, Shift and Alt are one key each.
    uint8_t NormalizeKey(uint8_t vk);
    bool MouseButton(uint8_t vk);

    // FFXI hides its pointer when a key sets off one of its commands; it stays when every key held is one to keep.
    bool KeepsPointer(const std::vector<uint8_t>& held, const std::vector<uint8_t>& keep);
    constexpr size_t kMaxKeptKeys = 32;

    std::string KeyName(uint8_t vk);

    // Every key down, once each and normalized, from a test of one key (GetAsyncKeyState in the plugin).
    template <typename Down>
    std::vector<uint8_t> HeldKeys(Down down)
    {
        constexpr uint8_t kFirstKey = 0x08, kLastKey = 0xFE, kFirstSided = 0xA0, kLastSided = 0xA5;
        std::vector<uint8_t> held;
        for (unsigned vk = kFirstKey; vk <= kLastKey; ++vk)
        {
            const auto key = static_cast<uint8_t>(vk);
            // The sided Ctrl, Shift and Alt are reported with the plain ones, which are kept.
            if (MouseButton(key) || (key >= kFirstSided && key <= kLastSided) || !down(key)) continue;
            held.push_back(key);
        }
        return held;
    }
}
