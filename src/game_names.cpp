#include "game_names.h"

#include "d3d_util.h"

namespace headsup
{
    bool GameNames::TargetScale::Read(IDirect3DSurface8* target, float backBufferWidth, float backBufferHeight)
    {
        D3DSURFACE_DESC desc{};
        if (FAILED(target->GetDesc(&desc)) || desc.Width == 0 || desc.Height == 0) return false;
        surface = reinterpret_cast<uintptr_t>(target);
        x       = backBufferWidth / static_cast<float>(desc.Width);
        y       = backBufferHeight / static_cast<float>(desc.Height);
        return true;
    }

    void GameNames::NewFrame()
    {
        FinishText();
        ++m_Frame;
        m_MeshDraws.clear();
        m_TextFinished   = false;
        m_NamesThisFrame = false;
        m_CameraSeen     = false;
        m_Scene.surface  = 0;
        ReadBackBufferSize();
    }

    void GameNames::FinishText()
    {
        if (m_TextFinished) return;
        m_TextFinished  = true;
        m_MeshDrawsLast = m_MeshDraws;
        for (const auto& [index, draws] : m_MeshDraws)
            m_LastMeshFrame[index] = m_Frame;
    }

    uint32_t GameNames::MeshDraws(uint16_t index) const
    {
        const auto it = m_MeshDrawsLast.find(index);
        return it == m_MeshDrawsLast.end() ? 0 : it->second;
    }

    uint32_t GameNames::FramesSinceMesh(uint16_t index) const
    {
        const auto it = m_LastMeshFrame.find(index);
        return it == m_LastMeshFrame.end() ? UINT32_MAX : m_Frame - it->second;
    }

    void GameNames::ReadBackBufferSize()
    {
        if (m_Device == nullptr) return;
        IDirect3DSurface8* back = nullptr;
        if (FAILED(m_Device->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &back)) || back == nullptr) return;
        m_BackBuffer = reinterpret_cast<uintptr_t>(back); // the swap chain keeps it alive
        D3DSURFACE_DESC desc{};
        if (SUCCEEDED(back->GetDesc(&desc)))
        {
            const float width = static_cast<float>(desc.Width), height = static_cast<float>(desc.Height);
            if (width != m_BackBufferWidth || height != m_BackBufferHeight) m_HaveCamera = false; // its pixels were another size
            m_BackBufferWidth  = width;
            m_BackBufferHeight = height;
        }
        back->Release();
    }

    void GameNames::CaptureCamera(const Tracker& tracker)
    {
        // Characters are drawn with vertex shaders, but each has fixed-function parts drawn with the scene's camera.
        if (m_CameraSeen || FindOwnerOnStack(tracker) == nullptr) return;
        D3DMATRIX view{}, projection{};
        if (FAILED(m_Device->GetTransform(D3DTS_VIEW, &view)) || FAILED(m_Device->GetTransform(D3DTS_PROJECTION, &projection)))
            return;
        m_Camera     = MakeCamera(&view.m[0][0], &projection.m[0][0], m_BackBufferHeight);
        m_CameraSeen = m_HaveCamera = true;
    }

    void GameNames::UseScene(IDirect3DSurface8* target, IDirect3DSurface8* depth)
    {
        if (target == nullptr || m_TextFinished) return;
        const uintptr_t image = reinterpret_cast<uintptr_t>(target);
        if (image != m_Scene.surface && !m_Scene.Read(target, m_BackBufferWidth, m_BackBufferHeight)) return;
        m_SceneDepth     = reinterpret_cast<uintptr_t>(depth);
        m_NamesThisFrame = true;
    }

    bool GameNames::SceneCopyStarting()
    {
        if (!TextPending() || m_Scene.surface == 0 || m_Scene.surface == m_BackBuffer) return false;
        IDirect3DSurface8* target = nullptr;
        if (FAILED(m_Device->GetRenderTarget(&target)) || target == nullptr) return false;
        const bool copying = reinterpret_cast<uintptr_t>(target) == m_BackBuffer;
        target->Release();
        return copying;
    }

    void GameNames::OnDrawUP(const Tracker& tracker, const Settings& settings)
    {
        if (!NameplatesOn(settings) || m_Device == nullptr || tracker.Actors().empty() || m_BackBufferWidth <= 0.0f) return;
        DWORD fvf = 0;
        if (!BoundFvf(m_Device, fvf)) return;
        if ((fvf & D3DFVF_POSITION_MASK) == D3DFVF_XYZ) CaptureCamera(tracker);
    }
}
