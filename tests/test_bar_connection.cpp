#include "bar_connection.h"
#include "test.h"

#include <algorithm>
#include <cmath>

using namespace headsup;

namespace
{
    bool Near(double a, double b) { return std::fabs(a - b) < 1e-6; }
}

TEST(latency_is_the_median_of_recent_round_trips)
{
    ConnectionMeter m;
    for (const double ms : {70.0, 50.0})
        m.RoundTrip(ms);
    CHECK(!m.Report(1.0, false).latency.has_value()); // waits for three
    m.RoundTrip(60.0);
    const ConnectionReport r = m.Report(1.0, false);
    CHECK(r.latency.has_value());
    CHECK(Near(r.latency->medianMs, 60.0) && Near(r.latency->lowestMs, 50.0) && Near(r.latency->highestMs, 70.0));
    // Only the last 15 count: once 15 more come, the first three are gone.
    for (size_t i = 0; i < kRoundTrips; ++i)
        m.RoundTrip(200.0);
    CHECK(Near(m.Report(1.0, false).latency->lowestMs, 200.0));
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

TEST(the_connection_widget_ends_in_the_games_own_meter_as_its_readout_does_unless_switched_off)
{
    ConnectionReport r;
    r.counters = NetCounters{1200, 300, 100};
    const auto joined = [](const WidgetContent& w) {
        std::string text;
        for (const Span& s : w.title)
            text += s.text;
        return text;
    };
    CHECK_EQ(joined(ConnectionWidget(r)), "S 300  R 1,200  100%");
    CHECK_EQ(joined(ConnectionWidget(r, false, false)), "S 300  R 1,200");
    // Under 100 it is the game's alarm, red as the rates are.
    r.counters->health = 40;
    const WidgetContent alarm = ConnectionWidget(r);
    CHECK(alarm.title.back().text == "  40%" && alarm.title.back().argb == kAlarmColor);
}

TEST(playonline_messages_waiting_add_the_games_envelope_and_messenger_each_with_its_count)
{
    ConnectionReport r;
    r.counters   = NetCounters{1200, 300, 100};
    r.playOnline = PlayOnlineCounts{2, 0}; // a message and a friend request, beside the messenger
    const WidgetContent w = ConnectionWidget(r);
    std::vector<GamePicture> pictures;
    std::string afterMail, afterMessenger;
    for (size_t i = 0; i < w.title.size(); ++i)
        if (w.title[i].picture != GamePicture::None)
        {
            pictures.push_back(w.title[i].picture);
            const std::string next = i + 1 < w.title.size() && w.title[i + 1].picture == GamePicture::None ? w.title[i + 1].text : "";
            (w.title[i].picture == GamePicture::PlayOnlineMail ? afterMail : afterMessenger) = next;
        }
    // As the game's Network window: both icons while either count is above 0, a count beside each that is.
    CHECK((pictures == std::vector<GamePicture>{GamePicture::PlayOnlineMail, GamePicture::PlayOnlineMessenger}));
    CHECK(afterMessenger == "2" && afterMail.empty());
    CHECK(w.title.front().text == "S "); // after the readout
    r.playOnline = PlayOnlineCounts{0, 0};
    const WidgetContent none = ConnectionWidget(r);
    CHECK(std::none_of(none.title.begin(), none.title.end(), [](const Span& s) { return s.picture != GamePicture::None; }));
    // The popup says it too, only while something waits.
    r.playOnline = PlayOnlineCounts{2, 1};
    const size_t sections = ConnectionPopup(r).sections.size();
    r.playOnline = PlayOnlineCounts{0, 0};
    CHECK_EQ(sections, ConnectionPopup(r).sections.size() + 1);
}
