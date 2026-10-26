#include "search_cipher.h"

#include <cstring>
#include <utility>
#include <vector>

namespace headsup
{
    namespace
    {
#include "ffxi_blowfish_tables.inc"
        static_assert(std::size(kBlowfishStart) == FfxiBlowfish::kRounds + 2 + FfxiBlowfish::kBoxes * FfxiBlowfish::kBoxSize);

        // RFC 1321: each round's shifts, and the constants, the integer part of 2^32 |sin(i + 1)|.
        constexpr int kShifts[64] = {7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 5, 9, 14, 20, 5, 9, 14,
            20, 5, 9, 14, 20, 5, 9, 14, 20, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 6, 10, 15, 21, 6,
            10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21};
        constexpr uint32_t kSines[64] = {0xD76AA478, 0xE8C7B756, 0x242070DB, 0xC1BDCEEE, 0xF57C0FAF, 0x4787C62A,
            0xA8304613, 0xFD469501, 0x698098D8, 0x8B44F7AF, 0xFFFF5BB1, 0x895CD7BE, 0x6B901122, 0xFD987193, 0xA679438E,
            0x49B40821, 0xF61E2562, 0xC040B340, 0x265E5A51, 0xE9B6C7AA, 0xD62F105D, 0x02441453, 0xD8A1E681, 0xE7D3FBC8,
            0x21E1CDE6, 0xC33707D6, 0xF4D50D87, 0x455A14ED, 0xA9E3E905, 0xFCEFA3F8, 0x676F02D9, 0x8D2A4C8A, 0xFFFA3942,
            0x8771F681, 0x6D9D6122, 0xFDE5380C, 0xA4BEEA44, 0x4BDECFA9, 0xF6BB4B60, 0xBEBFBC70, 0x289B7EC6, 0xEAA127FA,
            0xD4EF3085, 0x04881D05, 0xD9D4D039, 0xE6DB99E5, 0x1FA27CF8, 0xC4AC5665, 0xF4292244, 0x432AFF97, 0xAB9423A7,
            0xFC93A039, 0x655B59C3, 0x8F0CCC92, 0xFFEFF47D, 0x85845DD1, 0x6FA87E4F, 0xFE2CE6E0, 0xA3014314, 0x4E0811A1,
            0xF7537E82, 0xBD3AF235, 0x2AD7D2BB, 0xEB86D391};
        constexpr uint32_t kMd5Start[4] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476};
        constexpr size_t kMd5Block = 64, kMd5LengthBytes = 8;
        constexpr uint8_t kMd5Pad = 0x80;
        constexpr int kWordBits = 32, kByteBits = 8;

        uint32_t RotateLeft(uint32_t x, int n) { return x << n | x >> (kWordBits - n); }
    }

    std::array<uint8_t, kMd5Bytes> Md5(std::span<const uint8_t> data)
    {
        std::vector<uint8_t> message(data.begin(), data.end());
        const uint64_t bits = static_cast<uint64_t>(data.size()) * kByteBits;
        message.push_back(kMd5Pad);
        while (message.size() % kMd5Block != kMd5Block - kMd5LengthBytes)
            message.push_back(0);
        for (size_t i = 0; i < kMd5LengthBytes; ++i)
            message.push_back(static_cast<uint8_t>(bits >> (kByteBits * i)));
        uint32_t state[4] = {kMd5Start[0], kMd5Start[1], kMd5Start[2], kMd5Start[3]};
        for (size_t block = 0; block < message.size(); block += kMd5Block)
        {
            uint32_t x[16];
            std::memcpy(x, message.data() + block, kMd5Block);
            uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
            for (int i = 0; i < 64; ++i)
            {
                uint32_t f = 0;
                int k      = 0;
                switch (i / 16)
                {
                case 0: f = d ^ (b & (c ^ d)), k = i; break;
                case 1: f = c ^ (d & (b ^ c)), k = (5 * i + 1) % 16; break;
                case 2: f = b ^ c ^ d, k = (3 * i + 5) % 16; break;
                default: f = c ^ (b | ~d), k = (7 * i) % 16; break;
                }
                const uint32_t next = b + RotateLeft(a + f + x[k] + kSines[i], kShifts[i]);
                a                   = d;
                d                   = c;
                c                   = b;
                b                   = next;
            }
            state[0] += a;
            state[1] += b;
            state[2] += c;
            state[3] += d;
        }
        std::array<uint8_t, kMd5Bytes> digest{};
        std::memcpy(digest.data(), state, kMd5Bytes);
        return digest;
    }

    FfxiBlowfish::FfxiBlowfish(std::span<const uint8_t> key)
    {
        std::memcpy(m_P.data(), kBlowfishStart, sizeof(m_P));
        std::memcpy(m_S.data(), kBlowfishStart + m_P.size(), sizeof(m_S));
        size_t at = 0;
        for (uint32_t& p : m_P)
        {
            uint32_t word = 0;
            for (int k = 0; k < 4; ++k)
            {
                // The server's key is int8: a byte over 0x7F is sign-extended, filling the word's higher bits.
                word = word << kByteBits | static_cast<uint32_t>(static_cast<int32_t>(static_cast<int8_t>(key[at])));
                at   = (at + 1) % key.size();
            }
            p ^= word;
        }
        uint32_t left = 0, right = 0;
        for (size_t i = 0; i < m_P.size(); i += 2)
        {
            Encipher(left, right);
            m_P[i]     = left;
            m_P[i + 1] = right;
        }
        for (size_t i = 0; i < m_S.size(); i += 2)
        {
            Encipher(left, right);
            m_S[i]     = left;
            m_S[i + 1] = right;
        }
    }

    uint32_t FfxiBlowfish::Round(uint32_t x) const
    {
        constexpr uint32_t kOdd = 1, kQuirk = 32; // the server's round adds these where Schneier's xors two S-box words
        return ((m_S[kBoxSize + (x >> 8 & 0xFF)] & kOdd) ^ kQuirk) + ((m_S[3 * kBoxSize + (x >> 24)] & kOdd) ^ kQuirk) +
               m_S[2 * kBoxSize + (x >> 16 & 0xFF)] + m_S[x & 0xFF];
    }

    void FfxiBlowfish::Encipher(uint32_t& left, uint32_t& right) const
    {
        uint32_t l = left, r = right;
        for (int i = 0; i < kRounds; ++i)
        {
            l ^= m_P[static_cast<size_t>(i)];
            r ^= Round(l);
            std::swap(l, r);
        }
        std::swap(l, r);
        right = r ^ m_P[kRounds];
        left  = l ^ m_P[kRounds + 1];
    }

    void FfxiBlowfish::Decipher(uint32_t& left, uint32_t& right) const
    {
        uint32_t l = left, r = right;
        for (int i = kRounds + 1; i > 1; --i)
        {
            l ^= m_P[static_cast<size_t>(i)];
            r ^= Round(l);
            std::swap(l, r);
        }
        std::swap(l, r);
        right = r ^ m_P[1];
        left  = l ^ m_P[0];
    }
}
