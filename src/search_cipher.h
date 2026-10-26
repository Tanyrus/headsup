#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace headsup
{
    constexpr size_t kMd5Bytes = 16;
    std::array<uint8_t, kMd5Bytes> Md5(std::span<const uint8_t> data);

    // The Blowfish the game and its servers encrypt the search server's packets with, as LandSandBoat has it: not
    // Schneier's, as its round function adds the low bits of two S-box words, and a key byte is read signed.
    class FfxiBlowfish
    {
    public:
        explicit FfxiBlowfish(std::span<const uint8_t> key);
        void Encipher(uint32_t& left, uint32_t& right) const;
        void Decipher(uint32_t& left, uint32_t& right) const;

        static constexpr int kRounds = 16;
        static constexpr int kBoxSize = 256, kBoxes = 4;

    private:
        uint32_t Round(uint32_t x) const;
        std::array<uint32_t, kRounds + 2> m_P{};
        std::array<uint32_t, kBoxes * kBoxSize> m_S{};
    };
}
