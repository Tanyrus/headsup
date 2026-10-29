#pragma once

#include "bar_content.h"
#include "native_locate.h" // NetCounters

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>

namespace headsup
{
    constexpr double kQuietSeconds     = 2.0;  // server silence the bar shows in amber
    constexpr size_t kRoundTrips       = 15;   // the pings latency is taken over
    constexpr size_t kLeastRoundTrips  = 3;    // before it is shown
    constexpr double kLongestFrame     = 1.0;  // seconds: a longer step between fed frames counts nothing to the totals
    constexpr double kCounterSampling  = 0.25; // seconds between reads of the game's counters, which change every 3 s

    struct Latency
    {
        double medianMs = 0.0, lowestMs = 0.0, highestMs = 0.0;
        bool operator==(const Latency&) const = default;
    };

    struct ConnectionReport
    {
        std::optional<NetCounters> counters; // the game's, while it has a session and they could be read
        std::optional<double> silence;       // seconds since the last server packet; nothing before one
        bool zoning = false;                 // the game is changing zones, when silence is expected
        std::optional<Latency> latency;
        double receivedBytes = 0.0, sentBytes = 0.0; // since HeadsUp first read the counters
        double countedSeconds = 0.0;                 // how long those totals ran
        std::optional<PlayOnlineCounts> playOnline;
        bool operator==(const ConnectionReport&) const = default;
    };

    enum class LinkState : uint8_t
    {
        Normal,
        Quiet,  // nothing from the server for more than kQuietSeconds
        Silent, // the game's own meter is under 100: 10 seconds or more without server data
    };
    // Normal while zoning.
    LinkState StateOf(const ConnectionReport& report);

    // What HeadsUp measures of the connection itself.
    class ConnectionMeter
    {
    public:
        // Each packet the game receives, at now (seconds).
        void Received(double now);
        // Each ping's round trip, in milliseconds.
        void RoundTrip(double ms);
        // Each frame, with the game's counters when they could be read. Fed only while the bar runs with Connection
        // shown, so a step longer than kLongestFrame is time away (a logout, the widget off) and is not counted.
        void Frame(double now, const std::optional<NetCounters>& counters);
        ConnectionReport Report(double now, bool zoning) const;

    private:
        std::optional<double> m_LastReceived;
        std::optional<double> m_LastFrame;
        std::optional<NetCounters> m_Counters;
        std::deque<double> m_RoundTrips; // ms, oldest first
        std::optional<Latency> m_Latency; // from them, made as each arrives
        double m_ReceivedBytes = 0.0, m_SentBytes = 0.0, m_CountedSeconds = 0.0;
    };

    // "820 B", "34.5 KB", "1.2 MB", a KB being 1,024 bytes.
    std::string ByteText(double bytes);

    // latency: the median round trip after the rates, once there is one; meter: the game's own meter after them, as its
    // readout has it.
    WidgetContent ConnectionWidget(const std::optional<ConnectionReport>& report, bool latency = false, bool meter = true);
    PopupContent ConnectionPopup(const std::optional<ConnectionReport>& report);
}
