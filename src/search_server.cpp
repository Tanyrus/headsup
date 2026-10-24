#include "search_server.h"

#include "bytes.h"
#include "search_cipher.h"

#include <algorithm>
#include <cstring>

namespace headsup
{
    namespace
    {
        // A packet: its uint16 size at 0, "IXFF" at 4, its type at 0x0B, a body, the md5 of 8 up to the hash, the hash
        // and 4 key bytes. Encrypted from 8, in pairs of words, under md5 of kBaseKey and the key bytes; an answer's key
        // adds the 4 bytes before the request's hash.
        constexpr uint32_t kMagic             = 0x46465849; // "IXFF"
        constexpr size_t kMagicAt             = 0x04;
        constexpr size_t kTypeAt              = 0x0B;
        constexpr uint8_t kIdListType         = 0x01;
        constexpr size_t kIdCountAt           = 0x10, kIdsAt = 0x12;
        constexpr size_t kHashBytes           = 16, kNonceBytes = 4, kTrailerBytes = kHashBytes + kNonceBytes;
        constexpr size_t kEncryptedFrom       = 8;
        constexpr size_t kUnencryptedTail     = 12; // the words the cipher leaves at the end
        constexpr size_t kPacketAlign         = 4;
        constexpr size_t kMinPacket           = 28;
        constexpr size_t kRequestKeyBytes     = 20;
        constexpr std::array<uint8_t, 16> kBaseKey = {0x30, 0x73, 0x3D, 0x6D, 0x3C, 0x31, 0x49, 0x5A, 0x32, 0x7A, 0x42,
            0x43, 0x63, 0x38, 0x7B, 0x7E};
        // An answer: the size of its entries' end at 0x08, 0x80 at 0x0A on the last; entries from 0x18, each a size
        // byte and fields of a 5-bit code and bits of their own.
        constexpr size_t kDataEndAt     = 0x08;
        constexpr size_t kLastAt        = 0x0A;
        constexpr uint8_t kLastMark     = 0x80;
        constexpr size_t kEntriesAt     = 0x18;
        constexpr int kCodeBits         = 5;
        constexpr uint32_t kAnonymous   = 0x4000; // in flags1: the server sends no jobs or levels
        enum Code : uint8_t
        {
            kName          = 0x00,
            kArea          = 0x01,
            kNation        = 0x02,
            kJob           = 0x03,
            kLevel         = 0x04,
            kRace          = 0x05,
            kFlags1        = 0x06,
            kId            = 0x08,
            kLinkshellRank = 0x0D,
            kUnknown0E     = 0x0E,
            kRank          = 0x10,
            kComment       = 0x11,
            kFlags2        = 0x16,
            kLanguage      = 0x17,
        };
        constexpr int kNameLengthBits = 4, kNameCharBits = 7, kAreaBits = 10, kNationBits = 2, kJobBits = 5,
                      kLevelBits = 8, kRaceBits = 4, kFlags1Bits = 16, kIdBits = 20, kWordBits = 32, kRankBits = 8,
                      kLanguageBits = 16, kByteBits = 8;

        // The server reads its fields most significant bit first, across the bytes (utils.cpp, unpackBitsLE).
        class Bits
        {
        public:
            Bits(std::span<const uint8_t> data, size_t bit, size_t end) : m_Data(data), m_Bit(bit), m_End(end) {}
            bool Has(int bits) const { return m_Bit + static_cast<size_t>(bits) <= m_End; }
            // The zeros that fill an entry's last byte: up to 7, so they can look like the start of a name field.
            bool OnlyPadding() const
            {
                if (Has(kByteBits)) return false;
                for (size_t b = m_Bit; b < m_End; ++b)
                    if (m_Data[b / kByteBits] >> (kByteBits - 1 - b % kByteBits) & 1u) return false;
                return true;
            }
            uint32_t Read(int bits)
            {
                uint32_t value = 0;
                for (int i = 0; i < bits; ++i, ++m_Bit)
                    value = value << 1 | (m_Data[m_Bit / kByteBits] >> (kByteBits - 1 - m_Bit % kByteBits) & 1u);
                return value;
            }

        private:
            std::span<const uint8_t> m_Data;
            size_t m_Bit, m_End;
        };

        void Cipher(std::span<uint8_t> packet, std::span<const uint8_t> key, bool encrypt)
        {
            const std::array<uint8_t, kMd5Bytes> hash = Md5(key);
            const FfxiBlowfish fish(hash);
            size_t words = (packet.size() - kUnencryptedTail) / sizeof(uint32_t);
            words -= words % 2;
            for (size_t w = 0; w < words; w += 2)
            {
                const size_t at = kEncryptedFrom + w * sizeof(uint32_t);
                auto left = ReadAt<uint32_t>(packet.data(), at), right = ReadAt<uint32_t>(packet.data(), at + sizeof(uint32_t));
                encrypt ? fish.Encipher(left, right) : fish.Decipher(left, right);
                std::memcpy(packet.data() + at, &left, sizeof(left));
                std::memcpy(packet.data() + at + sizeof(left), &right, sizeof(right));
            }
        }

        std::optional<SearchedPlayer> ReadEntry(Bits bits)
        {
            SearchedPlayer p;
            uint32_t flags1 = kAnonymous;
            while (bits.Has(kCodeBits) && !bits.OnlyPadding())
            {
                const uint32_t code = bits.Read(kCodeBits);
                const auto field = [&](int width) { return bits.Has(width) ? std::optional(bits.Read(width)) : std::nullopt; };
                switch (code)
                {
                case kName:
                {
                    const std::optional<uint32_t> length = field(kNameLengthBits);
                    if (!length) return std::nullopt;
                    for (uint32_t c = 0; c < *length; ++c)
                        if (const auto letter = field(kNameCharBits)) p.name += static_cast<char>(*letter);
                        else return std::nullopt;
                    break;
                }
                case kArea:
                    if (const auto zone = field(kAreaBits)) p.zone = static_cast<uint16_t>(*zone);
                    break;
                case kJob:
                {
                    const auto main = field(kJobBits), sub = field(kJobBits);
                    if (!main || !sub) return std::nullopt;
                    p.mainJob = static_cast<uint8_t>(*main);
                    p.subJob  = static_cast<uint8_t>(*sub);
                    break;
                }
                case kLevel:
                {
                    const auto main = field(kLevelBits), sub = field(kLevelBits);
                    if (!main || !sub) return std::nullopt;
                    p.mainLevel = static_cast<uint8_t>(*main);
                    p.subLevel  = static_cast<uint8_t>(*sub);
                    break;
                }
                case kFlags1:
                    if (const auto flags = field(kFlags1Bits)) flags1 = *flags;
                    break;
                case kId:
                    if (const auto id = field(kIdBits)) p.id = *id;
                    break;
                case kNation: field(kNationBits); break;
                case kRace: field(kRaceBits); break;
                case kRank:
                case kLinkshellRank: field(kRankBits); break;
                case kUnknown0E:
                case kComment:
                case kFlags2: field(kWordBits); break;
                case kLanguage: field(kLanguageBits); break;
                default: return p.name.empty() ? std::nullopt : std::optional(p); // a field this reader does not know ends it
                }
            }
            p.anonymous = (flags1 & kAnonymous) != 0;
            if (p.anonymous) p.mainJob = p.subJob = p.mainLevel = p.subLevel = 0;
            if (p.name.empty() || p.id == 0) return std::nullopt;
            return p;
        }
    }

    SearchRequest IdListRequest(std::span<const uint32_t> ids, uint32_t nonce)
    {
        const size_t count = std::min(ids.size(), kMaxIdsPerRequest);
        size_t length      = kIdsAt + count * sizeof(uint32_t) + kTrailerBytes;
        length             = (length + kPacketAlign - 1) / kPacketAlign * kPacketAlign;
        SearchRequest request;
        std::vector<uint8_t>& p = request.bytes;
        p.assign(length, 0);
        const auto put = [&](size_t at, auto value) { std::memcpy(p.data() + at, &value, sizeof(value)); };
        put(0, static_cast<uint16_t>(length));
        put(kMagicAt, kMagic);
        p[kTypeAt] = kIdListType;
        put(kIdCountAt, static_cast<uint16_t>(count));
        for (size_t i = 0; i < count; ++i)
            put(kIdsAt + i * sizeof(uint32_t), ids[i]);
        put(length - kNonceBytes, nonce);
        const std::array<uint8_t, kMd5Bytes> hash =
            Md5(std::span<const uint8_t>(p).subspan(kEncryptedFrom, length - kEncryptedFrom - kTrailerBytes));
        std::copy(hash.begin(), hash.end(), p.begin() + static_cast<std::ptrdiff_t>(length - kTrailerBytes));
        std::array<uint8_t, kAnswerKeyBytes>& key = request.answerKey;
        std::copy(kBaseKey.begin(), kBaseKey.end(), key.begin());
        std::memcpy(key.data() + kBaseKey.size(), &nonce, kNonceBytes);
        std::memcpy(key.data() + kRequestKeyBytes, p.data() + length - kTrailerBytes - kNonceBytes, kNonceBytes);
        Cipher(p, std::span<const uint8_t>(key).first(kRequestKeyBytes), true);
        return request;
    }

    std::optional<size_t> SearchPacketSize(std::span<const uint8_t> head)
    {
        if (head.size() < sizeof(uint16_t)) return std::nullopt;
        const size_t size = ReadAt<uint16_t>(head.data(), 0);
        if (size < kMinPacket || size > kMaxSearchPacket) return std::nullopt;
        return size;
    }

    std::optional<SearchPacket> ReadSearchPacket(std::span<const uint8_t> packet, const std::array<uint8_t, kAnswerKeyBytes>& key)
    {
        const std::optional<size_t> size = SearchPacketSize(packet);
        if (!size || *size != packet.size() || ReadAt<uint32_t>(packet.data(), kMagicAt) != kMagic) return std::nullopt;
        std::vector<uint8_t> plain(packet.begin(), packet.end());
        Cipher(plain, key, false);
        const size_t hashAt = plain.size() - kTrailerBytes;
        const std::array<uint8_t, kMd5Bytes> hash = Md5(std::span<const uint8_t>(plain).subspan(kEncryptedFrom, hashAt - kEncryptedFrom));
        if (!std::equal(hash.begin(), hash.end(), plain.begin() + static_cast<std::ptrdiff_t>(hashAt))) return std::nullopt;
        const size_t end = ReadAt<uint16_t>(plain.data(), kDataEndAt);
        if (end > hashAt) return std::nullopt;
        SearchPacket answer;
        answer.last = plain[kLastAt] == kLastMark;
        for (size_t at = kEntriesAt; at < end;)
        {
            const size_t entry = plain[at];
            if (entry == 0 || at + 1 + entry > end) break;
            if (const std::optional<SearchedPlayer> player = ReadEntry(Bits(plain, (at + 1) * kByteBits, (at + 1 + entry) * kByteBits)))
                answer.players.push_back(*player);
            at += 1 + entry;
        }
        return answer;
    }
}
