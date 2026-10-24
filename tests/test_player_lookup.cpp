#include "player_lookup.h"
#include "test.h"

#include <vector>

using namespace headsup;

namespace
{
    using Ids = std::vector<uint32_t>;

    SearchedPlayer Answer(uint32_t id, uint8_t mainJob = 5, uint8_t mainLevel = 20)
    {
        return SearchedPlayer{id & kSearchIdMask, "Someone", 100, false, mainJob, 3, mainLevel, 10};
    }

    // Every one in view answered at once, as the server would.
    void AnswerAll(PlayerLookups& lookups, double now)
    {
        const std::optional<Ids> asked = lookups.NextRequest(now);
        std::vector<SearchedPlayer> answers;
        for (const uint32_t id : asked.value_or(Ids{}))
            answers.push_back(Answer(id));
        lookups.Answered(answers);
    }
}

TEST(players_in_view_are_asked_for_nearest_first_twenty_at_a_time_one_request_at_once)
{
    PlayerLookups lookups;
    std::vector<InView> view;
    for (uint32_t i = 0; i < 25; ++i)
        view.push_back(InView{1000 + i, static_cast<float>(30 - i)});
    lookups.Frame(view, 0.0);
    const std::optional<Ids> first = lookups.NextRequest(0.0);
    CHECK(first && first->size() == kMaxIdsPerRequest);
    CHECK_EQ(first->front(), 1024u); // the nearest
    CHECK(!lookups.NextRequest(0.5).has_value()); // the first is still out
    lookups.Answered({});
    const std::optional<Ids> second = lookups.NextRequest(1.0);
    CHECK(second && second->size() == 5u && second->front() == 1004u);
}

TEST(an_answer_is_kept_until_a_zone_change_or_thirty_minutes)
{
    PlayerLookups lookups;
    const std::vector<InView> view = {InView{1000, 5.0f}};
    lookups.Frame(view, 0.0);
    AnswerAll(lookups, 0.0);
    lookups.Frame(view, 600.0);
    CHECK(!lookups.NextRequest(600.0).has_value());
    lookups.Frame(view, PlayerLookups::kKeepSeconds + 1.0);
    CHECK((lookups.NextRequest(PlayerLookups::kKeepSeconds + 1.0) == Ids{1000}));
    lookups.Answered({});
    // A zone change asks again for whoever is in view after it; the old answer shows until the new one comes.
    lookups.Zoned();
    lookups.Frame(view, PlayerLookups::kKeepSeconds + 3.0);
    CHECK((lookups.NextRequest(PlayerLookups::kKeepSeconds + 3.0) == Ids{1000}));
}

TEST(an_answer_matches_by_the_ids_low_twenty_bits_and_whoever_it_leaves_out_waits)
{
    PlayerLookups lookups;
    constexpr uint32_t kBig = 0x00312345; // above 20 bits: the answer carries 0x12345
    lookups.Frame(std::vector<InView>{InView{kBig, 5.0f}, InView{2000, 6.0f}}, 0.0);
    const std::optional<Ids> asked = lookups.NextRequest(0.0);
    CHECK(asked && asked->size() == 2u);
    const std::vector<LookupAnswer> answered = lookups.Answered(std::vector<SearchedPlayer>{Answer(kBig)});
    CHECK(answered.size() == 1u && answered[0].id == kBig && answered[0].player.mainJob == 5);
    // 2000 was not online to the search server (or hidden from it): not asked again before the keep runs out.
    lookups.Frame(std::vector<InView>{InView{kBig, 5.0f}, InView{2000, 6.0f}}, 100.0);
    CHECK(!lookups.NextRequest(100.0).has_value());
}

TEST(a_failed_request_waits_then_asks_the_same_again)
{
    PlayerLookups lookups;
    lookups.Frame(std::vector<InView>{InView{1000, 5.0f}}, 0.0);
    CHECK(lookups.NextRequest(0.0).has_value());
    lookups.Failed(1.0);
    CHECK(!lookups.NextRequest(1.0 + PlayerLookups::kRetrySeconds - 1.0).has_value());
    CHECK((lookups.NextRequest(1.0 + PlayerLookups::kRetrySeconds) == Ids{1000}));
}

TEST(a_changed_look_or_a_return_from_away_asks_again_but_not_twice_a_minute)
{
    PlayerLookups lookups;
    const std::vector<InView> view = {InView{1000, 5.0f}};
    lookups.Frame(view, 0.0);
    AnswerAll(lookups, 0.0);
    // Gear changing as a job changes at a moogle.
    lookups.LookChanged(1000);
    CHECK(!lookups.NextRequest(30.0).has_value());
    lookups.LookChanged(1000);
    CHECK((lookups.NextRequest(PlayerLookups::kRecheckSeconds + 1.0) == Ids{1000}));
    AnswerAll(lookups, PlayerLookups::kRecheckSeconds + 1.0);
    // Out of view a short while: the same; long enough for a Mog House: asked again.
    double t = PlayerLookups::kRecheckSeconds * 3;
    lookups.Frame({}, t);
    lookups.Frame(view, t + 10.0);
    CHECK(!lookups.NextRequest(t + 10.0).has_value());
    lookups.Frame({}, t + 20.0);
    t += 20.0 + PlayerLookups::kAwaySeconds + 1.0;
    lookups.Frame(view, t);
    CHECK((lookups.NextRequest(t) == Ids{1000}));
}

TEST(nobody_out_of_view_is_asked_for)
{
    PlayerLookups lookups;
    lookups.Frame(std::vector<InView>{InView{1000, 5.0f}}, 0.0);
    lookups.Frame({}, 1.0);
    CHECK(!lookups.NextRequest(1.0).has_value());
    lookups.LookChanged(2000); // a look change of someone never in view
    CHECK(!lookups.NextRequest(2.0).has_value());
}
