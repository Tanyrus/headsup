#pragma once

#include "Ashita.h"
#include "settings.h"
#include "tracker.h"

#include <cstdint>

namespace headsup
{
    // Outlines mob bodies: draws a mob's mesh again, marking its pixels in the stencil buffer, then shifted solid copies
    // where the stencil is not that mob's.
    class OutlineRenderer
    {
    public:
        void SetDevice(IDirect3DDevice8* device) { m_Device = device; }
        void NewFrame();

        // Our own draws come back through the hooks, for the caller to ignore.
        bool Drawing() const { return m_InDraw; }
        const ActorInfo* CharacterMeshOwner(const Tracker& tracker);
        // True when it drew the mesh itself, and the caller must then block the game's draw.
        bool DrawOutlined(D3DPRIMITIVETYPE type, UINT minIndex, UINT numVertices, UINT startIndex, UINT primCount,
            const ActorInfo& owner, const Settings& settings);

        uint32_t MeshesLastFrame() const { return m_MeshesLast; }
        bool StencilAvailable() const { return m_StencilAvailable; }
        bool TakeStencilWarning();

    private:
        bool IsCharacterModelDraw();
        bool BoundSurfaceHasStencil();

        IDirect3DDevice8* m_Device   = nullptr;
        bool m_InDraw                = false;
        bool m_ClearedThisFrame      = false;
        bool m_StencilWarned         = false;
        bool m_StencilAvailable      = true;
        bool m_StencilWarningPending = false;
        uint32_t m_Meshes            = 0;
        uint32_t m_MeshesLast        = 0;
    };
}
