#pragma once

#include <windows.h>

namespace headsup
{
    // The module's import address slot that holds `function` from `library`; nullptr when it has none.
    void** ImportSlot(HMODULE module, const char* library, void* function);
    // The slot of its import from `library` by that library's ordinal, whatever another hook has put in it since.
    void** ImportSlotByOrdinal(HMODULE module, const char* library, WORD ordinal);
    // Points the slot at value; false when its page could not be made writable.
    bool WriteSlot(void** slot, void* value);
}
