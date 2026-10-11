#include "bar_connection.h"
#include "test.h"

#include <cmath>

using namespace headsup;

namespace
{
    bool Near(double a, double b) { return std::fabs(a - b) < 1e-6; }
}

TEST(a_reply_is_timed_from_the_first_send_to_the_next_packet_in_a_later_frame)
{
    ConnectionMeter m;
    for (int i = 0; i < 3; ++i)
    {
        const double t = i;
        m.Sent(t);
        m.Sent(t + 0.001);      // more of the same chunk
        m.Received(t + 0.002);  // in the same frame: it answered an earlier send
        m.Frame(t + 0.033, std::nullopt);
        m.Received(t + 0.05 + 0.01 * i); // the reply: 50, 60 and 70 ms
        m.Received(t + 0.051 + 0.01 * i); // the rest of its chunk
        m.Frame(t + 0.1, std::nullopt);
    }
    const ConnectionReport r = m.Report(3.0, false);
    CHECK(r.latency.has_value());
    CHECK(Near(r.latency->medianMs, 60.0) && Near(r.latency->lowestMs, 50.0) && Near(r.latency->highestMs, 70.0));
}

TEST(latency_waits_for_three_replies_and_drops_one_later_than_three_seconds)
{
    ConnectionMeter m;
    m.Sent(0.0);
    m.Frame(0.033, std::nullopt);
    m.Received(0.05);
    m.Sent(1.0);
    m.Frame(1.033, std::nullopt);
    m.Received(1.05);
    CHECK(!m.Report(1.1, false).latency.has_value());
    m.Sent(2.0);
    m.Frame(2.033, std::nullopt);
    m.Received(5.5); // its reply was lost: this answered a later send
    CHECK(!m.Report(5.6, false).latency.has_value());
}

TEST(silence_counts_from_the_last_packet_received)
{
    ConnectionMeter m;
    CHECK(!m.Report(10.0, false).silence.has_value());
    m.Received(10.0);
    CHECK(Near(*m.Report(12.5, false).silence, 2.5));
}

TEST(totals_add_up_the_games_rates_over_the_time_each_held)
{
    ConnectionMeter m;
    m.Frame(0.0, NetCounters{200, 100, 100});
    m.Frame(0.5, NetCounters{200, 100, 100});
    m.Frame(1.0, NetCounters{200, 100, 100});
    m.Frame(1.5, NetCounters{400, 100, 100});
    m.Frame(2.0, NetCounters{400, 100, 100});
    m.Frame(2.5, std::nullopt); // the session ended: nothing more counts
    const ConnectionReport r = m.Report(2.5, false);
    CHECK(Near(r.receivedBytes, 200 * 1.0 + 400 * 1.0) && Near(r.sentBytes, 100 * 2.0) && Near(r.countedSeconds, 2.0));
    CHECK(!r.counters.has_value());
}

TEST(a_gap_when_the_meter_was_not_fed_counts_nothing)
{
    // The meter is fed only while the bar runs with Connection shown: a logout to character select stops the bar.
    ConnectionMeter m;
    m.Frame(0.0, NetCounters{200, 100, 100});
    m.Frame(0.5, NetCounters{200, 100, 100});
    m.Frame(600.5, NetCounters{200, 100, 100}); // ten minutes later
    m.Frame(601.0, NetCounters{200, 100, 100});
    const ConnectionReport r = m.Report(601.0, false);
    CHECK(Near(r.receivedBytes, 200 * 1.0) && Near(r.sentBytes, 100 * 1.0) && Near(r.countedSeconds, 1.0));
}

TEST(the_link_is_quiet_after_two_seconds_without_data_and_silent_once_the_games_meter_drops)
{
    ConnectionReport r;
    r.counters = NetCounters{227, 175, 100};
    r.silence  = 1.5;
    CHECK(StateOf(r) == LinkState::Normal);
    r.silence = 2.5;
    CHECK(StateOf(r) == LinkState::Normal); // the game's counters still see data arrive
    r.counters->received = 0;
    CHECK(StateOf(r) == LinkState::Quiet);
    r.zoning = true; // changing zones: silence is expected
    CHECK(StateOf(r) == LinkState::Normal);
    r.zoning           = false;
    r.silence          = 13.0;
    r.counters->health = 94; // 10 s, then one 3-second update of -6
    CHECK(StateOf(r) == LinkState::Silent);
    r.counters.reset(); // without the counters, silence alone
    CHECK(StateOf(r) == LinkState::Quiet);
    CHECK(StateOf(ConnectionReport{}) == LinkState::Normal); // nothing yet: the title screen
}

TEST(bytes_read_in_b_kb_and_mb)
{
    CHECK_EQ(ByteText(820.0), std::string("820 B"));
    CHECK_EQ(ByteText(35328.0), std::string("34.5 KB"));
    CHECK_EQ(ByteText(1258291.0), std::string("1.2 MB"));
}
