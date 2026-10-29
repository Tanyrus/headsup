#include "import_slot.h"

#include <cstdint>

namespace headsup
{
    namespace
    {
        // The module's import descriptors, ending at one with no name.
        IMAGE_IMPORT_DESCRIPTOR* Imports(uint8_t* base)
        {
            const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(base + reinterpret_cast<const IMAGE_DOS_HEADER*>(base)->e_lfanew);
            const IMAGE_DATA_DIRECTORY& imports = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
            return imports.VirtualAddress == 0 ? nullptr : reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + imports.VirtualAddress);
        }
    }

    void** ImportSlot(HMODULE module, const char* library, void* function)
    {
        auto* base = reinterpret_cast<uint8_t*>(module);
        for (auto* d = Imports(base); d != nullptr && d->Name != 0; ++d)
        {
            if (_stricmp(reinterpret_cast<const char*>(base + d->Name), library) != 0) continue;
            for (auto* slot = reinterpret_cast<void**>(base + d->FirstThunk); *slot != nullptr; ++slot)
                if (*slot == function) return slot;
        }
        return nullptr;
    }

    void** ImportSlotByOrdinal(HMODULE module, const char* library, WORD ordinal)
    {
        auto* base = reinterpret_cast<uint8_t*>(module);
        for (auto* d = Imports(base); d != nullptr && d->Name != 0; ++d)
        {
            if (_stricmp(reinterpret_cast<const char*>(base + d->Name), library) != 0 || d->OriginalFirstThunk == 0) continue;
            // The lookup table, which the loader leaves as it was, runs beside the slots.
            auto* slot = reinterpret_cast<void**>(base + d->FirstThunk);
            for (auto* entry = reinterpret_cast<const IMAGE_THUNK_DATA32*>(base + d->OriginalFirstThunk); entry->u1.Ordinal != 0; ++entry, ++slot)
                if (IMAGE_SNAP_BY_ORDINAL32(entry->u1.Ordinal) && IMAGE_ORDINAL32(entry->u1.Ordinal) == ordinal) return slot;
        }
        return nullptr;
    }

    bool WriteSlot(void** slot, void* value)
    {
        DWORD old = 0;
        if (!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &old)) return false;
        *slot = value;
        VirtualProtect(slot, sizeof(void*), old, &old);
        return true;
    }
}
