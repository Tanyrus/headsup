#pragma once

#include "Ashita.h"
#include "outline_math.h"
#include "settings.h"
#include "tracker.h"

#include <cstdint>

namespace aggroglow
{
    // Outlines mob meshes from inside the plugin's DrawIndexedPrimitive callback (spec: Rendering).
    class OutlineRenderer
    {
    public:
        void SetDevice(IDirect3DDevice8* device) { m_Device = device; }

        // Called at Present: next frame clears stencil again; the mesh counter rolls over.
        void NewFrame();

        // Returns true when it drew the mesh itself; the caller must then block the original call.
        bool OnDrawIndexed(D3DPRIMITIVETYPE type, UINT minIndex, UINT numVertices, UINT startIndex, UINT primCount,
            const Tracker& tracker, const Settings& settings);

        uint32_t MeshesLastFrame() const { return m_MeshesLast; }
        bool StencilAvailable() const { return m_StencilAvailable; }
        // True once, the first time a mob could not be outlined because its depth buffer has no stencil bits.
        bool TakeStencilWarning();

    private:
        bool IsCharacterModelDraw();
        bool BoundSurfaceHasStencil();
        const ActorInfo* FindOwnerOnStack(const Tracker& tracker);

        IDirect3DDevice8* m_Device   = nullptr;
        bool m_InDraw                = false;
        bool m_ClearedThisFrame      = false;
        StencilCheck m_StencilCheck;
        bool m_StencilWarned         = false;
        bool m_StencilAvailable      = true;
        bool m_StencilWarningPending = false;
        uint32_t m_Meshes            = 0;
        uint32_t m_MeshesLast        = 0;
    };
}
