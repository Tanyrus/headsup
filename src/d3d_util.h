#pragma once

#include "Ashita.h"
#include "copy_state.h"
#include "tracker.h"

#include <cstdint>

namespace headsup
{
    bool IsDeclarationHandle(DWORD vertexShader);
    bool BoundFvf(IDirect3DDevice8* device, DWORD& fvf);
    // Only an identity to compare: its reference is released.
    uintptr_t BoundTexture(IDirect3DDevice8* device);

    CopyState ReadCopyState(IDirect3DDevice8* device);
    // texture: the one stage 0 had, which state.texture only names.
    void RestoreCopyState(IDirect3DDevice8* device, const CopyState& state, IDirect3DBaseTexture8* texture);

    // FindOwner over the current stack.
    const ActorInfo* FindOwnerOnStack(const Tracker& tracker);
}
