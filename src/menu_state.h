#pragma once

#include <optional>

namespace headsup
{
    constexpr double kSaveStallMs       = 20.0; // a save slower than a frame is deferred
    constexpr double kSaveSettleSeconds = 0.5;  // until this long after the last change
    // Whether the settings should be written now: a change is waiting, and either the last save was quick or the
    // change has settled.
    bool SaveDue(std::optional<double> changedAt, double now, double lastSaveMs);
}
