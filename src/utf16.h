#pragma once

#include <cstdint>
#include <string>

namespace headsup
{
    // UTF-8: every byte of a character after its first carries this tag in its top two bits, so text is cut only
    // before a byte without it.
    constexpr uint8_t kContinuationMask = 0xC0, kContinuationTag = 0x80;
    constexpr bool ContinuesCharacter(char byte)
    {
        return (static_cast<uint8_t>(byte) & kContinuationMask) == kContinuationTag;
    }

    // Any byte that is not valid UTF-8 becomes U+FFFD, so text always draws as something.
    std::u32string CodePoints(const std::string& utf8);
    std::wstring Utf16(const std::string& utf8);
}
