#include "outline.h"

#include "d3d_util.h"
#include "outline_math.h"

#include <cstring>
#include <iterator>
#include <utility>

namespace headsup
{
    namespace
    {
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
        m_MeshesLast       = std::exchange(m_Meshes, 0);
        m_ClearedThisFrame = false;
    }

    bool OutlineRenderer::TakeStencilWarning() { return std::exchange(m_StencilWarningPending, false); }

    // Characters are skinned straight into world space, so their world matrix is identity; the sun's shadow-map pass
    // binds no depth surface.
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

    // Read for every outlined mesh: a UI model preview can bind a depth surface without stencil while the scene's has it.
    bool OutlineRenderer::BoundSurfaceHasStencil()
    {
        IDirect3DSurface8* depth = nullptr;
        if (FAILED(m_Device->GetDepthStencilSurface(&depth)) || depth == nullptr) return false;
        D3DSURFACE_DESC desc{};
        const bool hasStencil = SUCCEEDED(depth->GetDesc(&desc)) &&
                                (desc.Format == D3DFMT_D15S1 || desc.Format == D3DFMT_D24S8 || desc.Format == D3DFMT_D24X4S4);
        depth->Release();
        return hasStencil;
    }

    const ActorInfo* OutlineRenderer::CharacterMeshOwner(const Tracker& tracker)
    {
        if (m_Device == nullptr || !IsCharacterModelDraw()) return nullptr;
        return FindOwnerOnStack(tracker);
    }

    bool OutlineRenderer::DrawOutlined(D3DPRIMITIVETYPE type, UINT minIndex, UINT numVertices, UINT startIndex, UINT primCount,
        const ActorInfo& owner, const Settings& settings)
    {
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

        m_Device->SetRenderState(D3DRS_STENCILENABLE, TRUE);
        m_Device->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_ALWAYS);
        m_Device->SetRenderState(D3DRS_STENCILREF, owner.stencilRef);
        m_Device->SetRenderState(D3DRS_STENCILMASK, 0xFF);
        m_Device->SetRenderState(D3DRS_STENCILWRITEMASK, 0xFF);
        m_Device->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_REPLACE);
        m_Device->SetRenderState(D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP);
        m_Device->SetRenderState(D3DRS_STENCILZFAIL, D3DSTENCILOP_KEEP);
        draw();

        m_Device->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_NOTEQUAL);
        m_Device->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_KEEP);
        m_Device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
        m_Device->SetRenderState(D3DRS_FOGENABLE, FALSE);
        m_Device->SetRenderState(D3DRS_TEXTUREFACTOR, owner.argb);
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

        m_Device->SetTransform(D3DTS_PROJECTION, &projection);
        for (size_t i = 0; i < std::size(kSavedStates); ++i)
            m_Device->SetRenderState(kSavedStates[i], states[i]);
        for (size_t i = 0; i < std::size(kSavedStages); ++i)
            m_Device->SetTextureStageState(kSavedStages[i].stage, kSavedStages[i].type, stages[i]);
        ++m_Meshes;
        return true;
    }
}
