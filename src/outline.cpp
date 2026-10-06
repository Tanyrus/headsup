#include "outline.h"

#include "outline_math.h"

#include <algorithm>
#include <cstring>
#include <iterator>

namespace aggroglow
{
    namespace
    {
        // D3D8 vertex declaration handles have bit 0 set (FVF codes never do); Wine also offsets them high.
        bool IsDeclarationHandle(DWORD vs)
        {
            return (vs & 1) != 0 || vs >= 0xF0000000;
        }

        Mat4 ToMat4(const D3DMATRIX& d)
        {
            Mat4 m;
            std::memcpy(m.m, &d, sizeof(m.m));
            return m;
        }

        D3DMATRIX ToD3D(const Mat4& m)
        {
            D3DMATRIX d;
            std::memcpy(&d, m.m, sizeof(m.m));
            return d;
        }

        const D3DRENDERSTATETYPE kSavedStates[] = {D3DRS_STENCILENABLE, D3DRS_STENCILFUNC, D3DRS_STENCILREF,
            D3DRS_STENCILMASK, D3DRS_STENCILWRITEMASK, D3DRS_STENCILPASS, D3DRS_STENCILFAIL, D3DRS_STENCILZFAIL,
            D3DRS_ZWRITEENABLE, D3DRS_FOGENABLE, D3DRS_TEXTUREFACTOR};

        struct StageState
        {
            DWORD stage;
            D3DTEXTURESTAGESTATETYPE type;
        };
        const StageState kSavedStages[] = {{0, D3DTSS_COLOROP}, {0, D3DTSS_COLORARG1}, {0, D3DTSS_ALPHAOP},
            {0, D3DTSS_ALPHAARG1}, {1, D3DTSS_COLOROP}, {1, D3DTSS_ALPHAOP}};
    }

    void OutlineRenderer::NewFrame()
    {
        m_MeshesLast       = m_Meshes;
        m_Meshes           = 0;
        m_ClearedThisFrame = false;
    }

    bool OutlineRenderer::TakeStencilWarning()
    {
        const bool pending      = m_StencilWarningPending;
        m_StencilWarningPending = false;
        return pending;
    }

    // Character model meshes: indexed, a vertex declaration handle, an identity world matrix (CPU-skinned straight
    // into world space) and a depth-stencil surface bound (the sun/shadow-map pass renders without one).
    bool OutlineRenderer::IsCharacterModelDraw()
    {
        DWORD vs = 0;
        if (FAILED(m_Device->GetVertexShader(&vs)) || !IsDeclarationHandle(vs)) return false;
        D3DMATRIX world{};
        if (FAILED(m_Device->GetTransform(D3DTS_WORLD, &world)) || !IsIdentity(ToMat4(world))) return false;
        IDirect3DSurface8* depth = nullptr;
        if (FAILED(m_Device->GetDepthStencilSurface(&depth)) || depth == nullptr) return false;
        depth->Release();
        return true;
    }

    // Per bound surface: a UI model preview can bind a depth surface without stencil while the scene's has it.
    bool OutlineRenderer::BoundSurfaceHasStencil()
    {
        IDirect3DSurface8* depth = nullptr;
        if (FAILED(m_Device->GetDepthStencilSurface(&depth)) || depth == nullptr) return false;
        const bool hasStencil = m_StencilCheck.HasStencil(reinterpret_cast<uintptr_t>(depth), [&] {
            D3DSURFACE_DESC desc{};
            depth->GetDesc(&desc);
            return static_cast<uint32_t>(desc.Format);
        });
        depth->Release();
        return hasStencil;
    }

    // noinline: the scan starts at this frame; above it sit Ashita's hook and the game's draw call chain.
    __attribute__((noinline)) const ActorInfo* OutlineRenderer::FindOwnerOnStack(const Tracker& tracker)
    {
        const auto sp       = reinterpret_cast<uintptr_t>(__builtin_frame_address(0));
        const auto* tib     = reinterpret_cast<const NT_TIB*>(NtCurrentTeb());
        const uintptr_t end = std::min<uintptr_t>(reinterpret_cast<uintptr_t>(tib->StackBase), sp + 0x1000);
        return FindOwner(reinterpret_cast<const uint32_t*>(sp & ~uintptr_t{3}), reinterpret_cast<const uint32_t*>(end & ~uintptr_t{3}), tracker);
    }

    bool OutlineRenderer::OnDrawIndexed(D3DPRIMITIVETYPE type, UINT minIndex, UINT numVertices, UINT startIndex,
        UINT primCount, const Tracker& tracker, const Settings& settings)
    {
        if (m_InDraw || m_Device == nullptr || !settings.enabled || tracker.OutlinedCount() == 0) return false;
        if (!IsCharacterModelDraw()) return false;
        const ActorInfo* owner = FindOwnerOnStack(tracker);
        if (owner == nullptr || !owner->outline) return false;
        m_StencilAvailable = BoundSurfaceHasStencil();
        if (!m_StencilAvailable)
        {
            if (!m_StencilWarned) m_StencilWarned = m_StencilWarningPending = true;
            return false;
        }

        if (!m_ClearedThisFrame)
        {
            m_Device->Clear(0, nullptr, D3DCLEAR_STENCIL, 0, 1.0f, 0); // the game never uses stencil
            m_ClearedThisFrame = true;
        }

        DWORD states[std::size(kSavedStates)];
        DWORD stages[std::size(kSavedStages)];
        for (size_t i = 0; i < std::size(kSavedStates); ++i)
            m_Device->GetRenderState(kSavedStates[i], &states[i]);
        for (size_t i = 0; i < std::size(kSavedStages); ++i)
            m_Device->GetTextureStageState(kSavedStages[i].stage, kSavedStages[i].type, &stages[i]);
        D3DMATRIX projection{};
        m_Device->GetTransform(D3DTS_PROJECTION, &projection);
        D3DVIEWPORT8 viewport{};
        m_Device->GetViewport(&viewport);

        m_InDraw  = true;
        auto draw = [&] { m_Device->DrawIndexedPrimitive(type, minIndex, numVertices, startIndex, primCount); };

        // 1. The mesh as the game drew it, also marking its visible pixels with this mob's stencil value.
        m_Device->SetRenderState(D3DRS_STENCILENABLE, TRUE);
        m_Device->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_ALWAYS);
        m_Device->SetRenderState(D3DRS_STENCILREF, owner->stencilRef);
        m_Device->SetRenderState(D3DRS_STENCILMASK, 0xFF);
        m_Device->SetRenderState(D3DRS_STENCILWRITEMASK, 0xFF);
        m_Device->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_REPLACE);
        m_Device->SetRenderState(D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP);
        m_Device->SetRenderState(D3DRS_STENCILZFAIL, D3DSTENCILOP_KEEP);
        draw();

        // 2. Solid-colour copies shifted around the silhouette wherever the stencil is not this mob.
        m_Device->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_NOTEQUAL);
        m_Device->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_KEEP);
        m_Device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
        m_Device->SetRenderState(D3DRS_FOGENABLE, FALSE);
        m_Device->SetRenderState(D3DRS_TEXTUREFACTOR, owner->argb);
        m_Device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
        m_Device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TFACTOR);
        m_Device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
        m_Device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE); // keeps alpha-tested cut-outs
        m_Device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
        m_Device->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
        const Mat4 base = ToMat4(projection);
        for (int tap = 0; tap < settings.smoothness; ++tap)
        {
            float dx = 0.0f, dy = 0.0f;
            OutlineOffset(tap, settings.smoothness, settings.thickness, static_cast<float>(viewport.Width),
                static_cast<float>(viewport.Height), dx, dy);
            const D3DMATRIX shifted = ToD3D(ShiftProjection(base, dx, dy));
            m_Device->SetTransform(D3DTS_PROJECTION, &shifted);
            draw();
        }
        m_InDraw = false;

        // 3. Restore everything this touched.
        m_Device->SetTransform(D3DTS_PROJECTION, &projection);
        for (size_t i = 0; i < std::size(kSavedStates); ++i)
            m_Device->SetRenderState(kSavedStates[i], states[i]);
        for (size_t i = 0; i < std::size(kSavedStages); ++i)
            m_Device->SetTextureStageState(kSavedStages[i].stage, kSavedStages[i].type, stages[i]);
        ++m_Meshes;
        return true;
    }
}
