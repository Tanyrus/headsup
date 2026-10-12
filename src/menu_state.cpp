#include "menu_state.h"

namespace headsup
{
    bool SaveDue(std::optional<double> changedAt, double now, double lastSaveMs)
    {
        if (!changedAt) return false;
        return lastSaveMs <= kSaveStallMs || now - *changedAt >= kSaveSettleSeconds;
    }
}
