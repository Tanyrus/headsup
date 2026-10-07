#pragma once

#include "Ashita.h"
#include "tracker.h"

#include <cstdint>

namespace headsup
{
    // A D3D8 vertex declaration handle rather than an FVF code.
    bool IsDeclarationHandle(DWORD vertexShader);
    // The bound vertex format of a fixed-function draw; false for a declaration handle or when it cannot be read.
    bool BoundFvf(IDirect3DDevice8* device, DWORD& fvf);
    // The texture bound to stage 0, as an identity to compare; 0 for none.
    uintptr_t BoundTexture(IDirect3DDevice8* device);

    // The entity whose draw is running: the first tracked actor pointer on the stack. That is the live actor being
    // drawn; pointers further up can be stale leftovers.
    const ActorInfo* FindOwnerOnStack(const Tracker& tracker);

    // A render target's pixels to the back buffer's.
    struct TargetScale
    {
        uintptr_t surface = 0; // the target it was read for; 0 to read again (a target made at the same address may differ)
        float x = 1.0f, y = 1.0f;
        // False, keeping the last scale, when the target's size cannot be read.
        bool Read(IDirect3DSurface8* target, float backBufferWidth, float backBufferHeight);
    };
}
