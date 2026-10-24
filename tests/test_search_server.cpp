#include "search_cipher.h"
#include "search_server.h"
#include "test.h"

#include <array>
#include <string>
#include <vector>

using namespace headsup;

namespace
{
    std::vector<uint8_t> Bytes(const std::string& hex)
    {
        std::vector<uint8_t> out;
        for (size_t i = 0; i + 1 < hex.size(); i += 2)
            out.push_back(static_cast<uint8_t>(std::stoi(hex.substr(i, 2), nullptr, 16)));
        return out;
    }
    std::string Hex(const std::array<uint8_t, 16>& bytes)
    {
        std::string out;
        for (const uint8_t b : bytes)
        {
            constexpr const char* kDigits = "0123456789abcdef";
            out += kDigits[b >> 4];
            out += kDigits[b & 0xF];
        }
        return out;
    }

    // Recorded on the test server (2026-10-23) by ~/.cache/headsup-feedback-7/player-lookup/fixtures/record.cpp, which
    // builds a request through the server's own blowfish and md5 with the nonce 0x12345678: Headsup (65001, RDM 20 /
    // WHM 10 in West Ronfaure), then Headsup anonymous with two ids of no one online.
    const std::string kHeadsupRequest = "2c00000049584646d56a8e3f09a4af618f1aa3e83c0201014540ed79e104de7fec1547d2c5ca210378563412";
    const std::string kHeadsupKey     = "30733d6d3c31495a327a424363387b7e7856341200000000";
    const std::string kHeadsupAnswer  = "50000000495846464a4858e4042a6d70d3f69548939e732eb67987e2cb99df179d21a5cb2385affb9f610ae8caa7616708887ee7e16a928354d8779eb00258d866cfcadffd25efba3273b9e978563412";
    const std::string kAnonRequest    = "3400000049584646d56a8e3f09a4af6151cd5a899fb6276e045c3dae0fa377c85fcb1ca5d20b976e644852ba593556b478563412";
    const std::string kAnonKey        = "30733d6d3c31495a327a424363387b7e785634120f000000";
    const std::string kAnonAnswer     = "48000000495846464ecc1dc544824bb9b7f9370b1d79ada20b41177e8d8ea80ce9e48a5619be422ddbeae0fe13ab5d206d2dacd504f0a7145dd28210907d86c19df7554b78563412";
    constexpr uint32_t kNonce = 0x12345678;
    constexpr uint32_t kHeadsup = 65001;

    std::array<uint8_t, kAnswerKeyBytes> Key(const std::string& hex)
    {
        std::array<uint8_t, kAnswerKeyBytes> key{};
        const std::vector<uint8_t> bytes = Bytes(hex);
        std::copy(bytes.begin(), bytes.end(), key.begin());
        return key;
    }
}

TEST(md5_is_rfc_1321s)
{
    CHECK_EQ(Hex(Md5({})), std::string("d41d8cd98f00b204e9800998ecf8427e"));
    const std::string abc = "abc";
    CHECK_EQ(Hex(Md5({reinterpret_cast<const uint8_t*>(abc.data()), abc.size()})), std::string("900150983cd24fb0d6963f7d28e17f72"));
    // Past one 64-byte block: RFC 1321's 80 digits.
    std::string digits;
    for (int i = 0; i < 8; ++i)
        digits += "1234567890";
    CHECK_EQ(Hex(Md5({reinterpret_cast<const uint8_t*>(digits.data()), digits.size()})), std::string("57edf4a22be3c955ac49da2e2107b67a"));
}

TEST(an_id_list_request_is_the_bytes_the_server_builds)
{
    const std::array<uint32_t, 1> one{kHeadsup};
    const SearchRequest request = IdListRequest(one, kNonce);
    CHECK(request.bytes == Bytes(kHeadsupRequest));
    CHECK(request.answerKey == Key(kHeadsupKey));
    const std::array<uint32_t, 3> three{kHeadsup, kHeadsup + 1, 999999};
    const SearchRequest more = IdListRequest(three, kNonce);
    CHECK(more.bytes == Bytes(kAnonRequest) && more.answerKey == Key(kAnonKey));
}

TEST(an_answer_gives_each_players_jobs_and_levels)
{
    const std::vector<uint8_t> answer = Bytes(kHeadsupAnswer);
    CHECK(SearchPacketSize(answer) == answer.size());
    const std::optional<SearchPacket> packet = ReadSearchPacket(answer, Key(kHeadsupKey));
    CHECK(packet && packet->last && packet->players.size() == 1u);
    const SearchedPlayer& p = packet->players[0];
    constexpr uint8_t kRdm = 5, kWhm = 3;
    constexpr uint16_t kWestRonfaure = 100;
    CHECK_EQ(p.id, kHeadsup & kSearchIdMask);
    CHECK_EQ(p.name, std::string("Headsup"));
    CHECK(!p.anonymous && p.zone == kWestRonfaure);
    CHECK(p.mainJob == kRdm && p.mainLevel == 20 && p.subJob == kWhm && p.subLevel == 10);
}

TEST(an_anonymous_player_answers_without_jobs_and_no_one_offline_answers)
{
    const std::optional<SearchPacket> packet = ReadSearchPacket(Bytes(kAnonAnswer), Key(kAnonKey));
    CHECK(packet && packet->last && packet->players.size() == 1u);
    const SearchedPlayer& p = packet->players[0];
    CHECK(p.anonymous && p.name == std::string("Headsup") && p.mainJob == 0 && p.mainLevel == 0);
}

TEST(a_packet_cut_short_or_under_another_key_is_no_answer)
{
    const std::vector<uint8_t> answer = Bytes(kHeadsupAnswer);
    CHECK(!ReadSearchPacket(std::span(answer).first(answer.size() - 8), Key(kHeadsupKey)));
    CHECK(!ReadSearchPacket(answer, Key(kAnonKey)));
    CHECK(!ReadSearchPacket({}, Key(kHeadsupKey)));
    const std::vector<uint8_t> tiny = {4, 0, 0, 0};
    CHECK(!SearchPacketSize(tiny).has_value());
}
