#include "game_names.h"

#include "bytes.h"
#include "d3d_util.h"

#include <optional>

namespace headsup
{
    namespace
    {
        std::optional<uint32_t> FirstVertexColor(DWORD fvf, const void* vertices, UINT stride)
        {
            if ((fvf & D3DFVF_DIFFUSE) == 0 || stride < kPretransformedPositionBytes + sizeof(uint32_t)) return std::nullopt;
            return ReadAt<uint32_t>(static_cast<const uint8_t*>(vertices), kPretransformedPositionBytes);
        }
    }

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
        m_TextureUse.clear();
        m_MeshDraws.clear();
        m_TextFinished   = false;
        m_NamesThisFrame = false;
        m_CameraSeen     = false;
        m_Scene.surface  = 0;
        m_Ui.surface     = 0;
        ReadBackBufferSize();
    }

    void GameNames::FinishText()
    {
        if (m_TextFinished) return;
        m_TextFinished  = true;
        m_Font          = MostUsedTexture(m_TextureUse, m_Font);
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

    void GameNames::OnDrawUP(D3DPRIMITIVETYPE type, UINT primCount, const void* vertices, UINT stride, const Tracker& tracker,
        const Settings& settings)
    {
        if (!NameplatesOn(settings) || m_Device == nullptr || tracker.Actors().empty() || m_BackBufferWidth <= 0.0f) return;
        DWORD fvf = 0;
        if (!BoundFvf(m_Device, fvf)) return;
        if ((fvf & D3DFVF_POSITION_MASK) == D3DFVF_XYZ)
        {
            CaptureCamera(tracker);
            return;
        }
        // Letters are textured; the quad the game draws over each character is not.
        if ((fvf & D3DFVF_POSITION_MASK) != D3DFVF_XYZRHW || (fvf & D3DFVF_TEXCOUNT_MASK) == 0) return;
        ScreenBox box;
        float depth = 0.0f;
        if (!WorldTextBox(vertices, stride, VertexCount(type, primCount), box, depth)) return; // cheap: rejects HUD text
        if (FindOwnerOnStack(tracker) != nullptr) ++m_TextureUse[BoundTexture(m_Device)];
    }

    bool GameNames::IsGameCursorDraw(D3DPRIMITIVETYPE type, UINT primCount, const void* vertices, UINT stride,
        const Tracker& tracker, const std::vector<CursorName>& names, bool picking, ITarget* target)
    {
        if (names.empty() || m_Device == nullptr || m_BackBufferWidth <= 0.0f || VertexCount(type, primCount) != kQuadVertices)
            return false;
        DWORD fvf = 0;
        if (!BoundFvf(m_Device, fvf) || (fvf & D3DFVF_POSITION_MASK) != D3DFVF_XYZRHW) return false;
        ScreenBox ui;
        if (!UiQuadBox(vertices, stride, ui)) return false;
        IDirect3DSurface8* surface = nullptr;
        if (FAILED(m_Device->GetRenderTarget(&surface)) || surface == nullptr) return false;
        const bool scaled =
            reinterpret_cast<uintptr_t>(surface) == m_Ui.surface || m_Ui.Read(surface, m_BackBufferWidth, m_BackBufferHeight);
        surface->Release();
        if (!scaled) return false;
        const CursorQuad quad{ui, ui.Scaled(m_Ui.x, m_Ui.y), BoundTexture(m_Device)};
        std::vector<CursorAnchor> anchors;
        if (const Ashita::FFXI::targetwindow_t* window = target != nullptr ? target->GetRawStructureWindow() : nullptr)
            anchors = GameCursorAnchors(names,
                CursorWindow{static_cast<float>(window->m_AnkX), static_cast<float>(window->m_AnkY),
                    static_cast<float>(window->m_SubAnkX), static_cast<float>(window->m_SubAnkY)},
                picking);
        if (!MayBeGameCursor(quad, m_ArrowTexture, anchors, names)) return false;
        const ActorInfo* owner        = FindOwnerOnStack(tracker);
        const CursorVerdict verdict   = JudgeGameCursor(quad, m_ArrowTexture, m_Font, anchors, names,
            owner != nullptr ? std::optional<uint16_t>(owner->index) : std::nullopt);
        if (verdict.learnArrow) m_ArrowTexture = quad.texture;
        if (verdict.block)
            if (const auto argb = FirstVertexColor(fvf, vertices, stride))
                if (const auto outOfRange = PickedOutOfRange(*argb)) m_PickOutOfRange = *outOfRange;
        return verdict.block;
    }
}
