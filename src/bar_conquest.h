#pragma once

#include "bar_content.h"
#include "game_packets.h"

#include <cstdint>
#include <optional>

namespace headsup
{
    // The tally from the clock, so it shows before the first conquest update; the points from that update.
    WidgetContent ConquestWidget(int64_t unixMs, const std::optional<ConquestStanding>& standing);
    PopupContent ConquestPopup(int64_t unixMs, const std::optional<ConquestStanding>& standing);
}
