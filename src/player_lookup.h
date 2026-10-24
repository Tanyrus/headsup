#pragma once

#include "search_server.h"

#include <cstdint>
#include <optional>
#include <span>
#include <unordered_map>
#include <vector>

namespace headsup
{
    // A player the server spawned for you: their character id and how far they stand.
    struct InView
    {
        uint32_t id;
        float distance;
    };
    // An answer matched to the character it is about.
    struct LookupAnswer
    {
        uint32_t id;
        SearchedPlayer player;
    };

    // Who to ask the search server about, and when. Each player in view is asked once, the nearest first, a
    // request at a time; an answer is kept until a zone change or kKeepSeconds. A player is asked again sooner when
    // their look changes (a job change takes off what the new job cannot wear) or they come back after kAwaySeconds
    // out of view (their Mog House), but never within kRecheckSeconds of the last time.
    class PlayerLookups
    {
    public:
        static constexpr double kKeepSeconds    = 30.0 * 60.0;
        static constexpr double kRecheckSeconds = 60.0;
        static constexpr double kAwaySeconds    = 60.0;
        static constexpr double kRetrySeconds   = 30.0; // after a request that failed

        void Frame(std::span<const InView> inView, double now);
        void LookChanged(uint32_t id);
        void Zoned();
        // The ids to ask about now, nearest first; none while a request is out, waiting after a failure, or when
        // no one in view needs asking.
        std::optional<std::vector<uint32_t>> NextRequest(double now);
        // The request out was answered: each player it names matched to the id asked for; the rest asked for are not
        // asked again until their keep runs out.
        std::vector<LookupAnswer> Answered(std::span<const SearchedPlayer> players);
        void Failed(double now);

    private:
        struct Player
        {
            std::optional<double> asked;     // when last asked
            std::optional<double> left;      // when they went out of view, while they are
            bool inView      = false;
            bool stale       = false;        // to ask again, as soon as kRecheckSeconds allows
            float distance   = 0.0f;
        };
        bool Due(const Player& p, double now) const;

        std::unordered_map<uint32_t, Player> m_Players;
        std::vector<uint32_t> m_Out; // the request out
        std::optional<double> m_RetryAt;
    };
}
