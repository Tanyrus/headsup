#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace headsup
{
    // The search server's ID list, the request the game's friend list makes: up to kMaxIdsPerRequest character ids, a
    // TCP connection each, answered in packets of up to as many players, the last marked. Layouts from LandSandBoat's
    // search_handler.cpp and packets/search_list.cpp; the server never tells the players asked about.
    constexpr size_t kMaxIdsPerRequest = 20;
    constexpr size_t kAnswerKeyBytes   = 24;
    // An answer names a player by the low 20 bits of their character id.
    constexpr uint32_t kSearchIdMask = 0xFFFFF;

    struct SearchRequest
    {
        std::vector<uint8_t> bytes; // to send as they are
        std::array<uint8_t, kAnswerKeyBytes> answerKey{};
    };
    // A request for these ids (at most kMaxIdsPerRequest are sent), the nonce its 4 key bytes.
    SearchRequest IdListRequest(std::span<const uint32_t> ids, uint32_t nonce);

    struct SearchedPlayer
    {
        uint32_t id = 0; // masked by kSearchIdMask
        std::string name;
        uint16_t zone  = 0;
        bool anonymous = true; // the server leaves out an anonymous player's nation, jobs, levels, race and rank
        uint8_t mainJob = 0, subJob = 0, mainLevel = 0, subLevel = 0;
        bool operator==(const SearchedPlayer&) const = default;
    };
    struct SearchPacket
    {
        bool last = false;
        std::vector<SearchedPlayer> players;
    };
    // A packet's size from its first two bytes: none for one shorter than any packet or longer than kMaxSearchPacket.
    constexpr size_t kMaxSearchPacket = 2048;
    std::optional<size_t> SearchPacketSize(std::span<const uint8_t> head);
    // One whole answer packet, decrypted with the request's key: none for one cut short, of another key or not a list.
    std::optional<SearchPacket> ReadSearchPacket(std::span<const uint8_t> packet, const std::array<uint8_t, kAnswerKeyBytes>& key);
}
