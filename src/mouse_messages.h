#pragma once

#include <cstdint>

namespace headsup
{
    // Windows' mouse messages and the button flags in their wParam, as winuser.h has them: this file is built without
    // its headers.
    constexpr uint32_t kMouseMove = 0x200, kLeftDown = 0x201, kLeftUp = 0x202, kLeftDouble = 0x203, kRightDown = 0x204,
                       kRightUp = 0x205, kRightDouble = 0x206, kMiddleDown = 0x207, kMiddleUp = 0x208,
                       kMiddleDouble = 0x209, kXButtonDown = 0x20B, kXButtonUp = 0x20C, kXButtonDouble = 0x20D;
    constexpr uint32_t kLeftButton = 0x1, kRightButton = 0x2;
}
