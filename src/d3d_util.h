#pragma once

#include "Ashita.h"
#include "tracker.h"

#include <cstdint>

namespace headsup
{
    bool IsDeclarationHandle(DWORD vertexShader);
    bool BoundFvf(IDirect3DDevice8* device, DWORD& fvf);
    // Only an identity to compare: its reference is released.
    uintptr_t BoundTexture(IDirect3DDevice8* device);

    // FindOwner over the current stack.
    const ActorInfo* FindOwnerOnStack(const Tracker& tracker);
}
