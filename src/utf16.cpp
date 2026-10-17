#include "utf16.h"

#include <cstdint>

namespace headsup
{
    namespace
    {
        constexpr char32_t kReplacement   = 0xFFFD;
        constexpr uint32_t kMaxCodePoint  = 0x10FFFF;
        // UTF-8: a lead byte's top bits give its sequence's length, and each continuation byte carries six bits.
        constexpr uint8_t kAsciiEnd          = 0x80;
        constexpr int kContinuationBits      = 6;
        constexpr uint32_t kContinuationBody = (1u << kContinuationBits) - 1;
        constexpr int kLeadBits              = 8;
        // UTF-16: a code point past U+FFFF is two surrogates, ten bits each.
        constexpr uint32_t kSurrogateBase = 0x10000;
        constexpr uint32_t kHighSurrogate = 0xD800;
        constexpr uint32_t kLowSurrogate  = 0xDC00;
        constexpr int kSurrogateBits      = 10;
        constexpr uint32_t kSurrogateBody = (1u << kSurrogateBits) - 1;

        struct LeadByte
        {
            uint8_t mask, tag;
            int length;
        };
        constexpr LeadByte kLeads[] = {{0xE0, 0xC0, 2}, {0xF0, 0xE0, 3}, {0xF8, 0xF0, 4}};

        // 0 for a byte that cannot start a sequence.
        int SequenceLength(uint8_t lead)
        {
            if (lead < kAsciiEnd) return 1;
            for (const LeadByte& l : kLeads)
                if ((lead & l.mask) == l.tag) return l.length;
            return 0;
        }
    }

    std::u32string CodePoints(const std::string& utf8)
    {
        std::u32string out;
        for (size_t i = 0; i < utf8.size();)
        {
            const auto lead  = static_cast<uint8_t>(utf8[i]);
            const int length = SequenceLength(lead);
            if (length == 0)
            {
                out.push_back(kReplacement);
                ++i;
                continue;
            }
            // A lead byte keeps the bits below its length's marker: 8 - (length + 1) of them.
            uint32_t code = length == 1 ? lead : static_cast<uint32_t>(lead & ((1u << (kLeadBits - length - 1)) - 1));
            size_t taken  = 1;
            for (; taken < static_cast<size_t>(length) && i + taken < utf8.size(); ++taken)
            {
                const auto next = static_cast<uint8_t>(utf8[i + taken]);
                if (!ContinuesCharacter(utf8[i + taken])) break;
                code = (code << kContinuationBits) | (next & kContinuationBody);
            }
            if (taken != static_cast<size_t>(length) || code > kMaxCodePoint)
            {
                out.push_back(kReplacement);
                i += taken;
                continue;
            }
            out.push_back(code);
            i += static_cast<size_t>(length);
        }
        return out;
    }

    std::wstring Utf16(const std::string& utf8)
    {
        std::wstring out;
        for (const char32_t code : CodePoints(utf8))
        {
            if (code >= kSurrogateBase)
            {
                const uint32_t rest = code - kSurrogateBase;
                out.push_back(static_cast<wchar_t>(kHighSurrogate + (rest >> kSurrogateBits)));
                out.push_back(static_cast<wchar_t>(kLowSurrogate + (rest & kSurrogateBody)));
            }
            else
                out.push_back(static_cast<wchar_t>(code));
        }
        return out;
    }
}
