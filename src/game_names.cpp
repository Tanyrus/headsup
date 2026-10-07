#include "game_names.h"

#include <cstring>

namespace headsup
{
    void GameNames::NewFrame()
    {
        FinishText();
        m_Glyphs.clear();
        m_KeptNow.clear();
        m_MeshDraws.clear();
        m_TextStats      = TextDrawStats{};
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
        m_Last          = ReadFrameNames(m_Glyphs, m_KeptNow, m_Last);
        m_MeshDrawsLast = m_MeshDraws;
        m_TextStatsLast = m_TextStats;
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

    bool GameNames::SceneCopyStarting()
    {
        if (!TextPending() || m_Scene.surface == 0 || m_Scene.surface == m_BackBuffer) return false;
        IDirect3DSurface8* target = nullptr;
        if (FAILED(m_Device->GetRenderTarget(&target)) || target == nullptr) return false;
        const bool copying = reinterpret_cast<uintptr_t>(target) == m_BackBuffer;
        target->Release();
        return copying;
    }

    bool GameNames::OnDrawUP(D3DPRIMITIVETYPE type, UINT primCount, const void* vertices, UINT stride, const Tracker& tracker,
        const Settings& settings, Blocking blocking)
    {
        if (!NameplatesOn(settings) || m_Device == nullptr || tracker.Actors().empty() || m_BackBufferWidth <= 0.0f) return false;
        DWORD fvf = 0;
        if (!BoundFvf(m_Device, fvf)) return false;
        if ((fvf & D3DFVF_POSITION_MASK) == D3DFVF_XYZ)
        {
            CaptureCamera(tracker);
            return false;
        }
        if ((fvf & D3DFVF_POSITION_MASK) != D3DFVF_XYZRHW) return false;
        ScreenBox box;
        float depth = 0.0f;
        if (!WorldTextBox(vertices, stride, VertexCount(type, primCount), box, depth)) return false; // cheap: rejects HUD text
        const uintptr_t texture = BoundTexture(m_Device);
        const bool letter       = m_Last.font == 0 || texture == m_Last.font;
        IDirect3DSurface8* target = nullptr;
        if (FAILED(m_Device->GetRenderTarget(&target)) || target == nullptr) return false;
        const uintptr_t image = reinterpret_cast<uintptr_t>(target);
        bool inNames          = InNamesImage(letter, image, m_NamesImage);
        if (inNames && image != m_Scene.surface)
        {
            inNames = m_Scene.Read(target, m_BackBufferWidth, m_BackBufferHeight);
            IDirect3DSurface8* sceneDepth = nullptr;
            m_SceneDepth = SUCCEEDED(m_Device->GetDepthStencilSurface(&sceneDepth)) ? reinterpret_cast<uintptr_t>(sceneDepth) : 0;
            if (sceneDepth != nullptr) sceneDepth->Release();
        }
        target->Release();
        if (!inNames) return false;
        ++m_TextStats.inScene;
        m_NamesThisFrame = true;

        // Pretransformed coordinates are render-target pixels; nameplates are placed in back-buffer pixels.
        const ScreenBox glyph      = box.Scaled(m_Scene.x, m_Scene.y);
        const ActorInfo* owner     = FindOwnerOnStack(tracker);
        bool replaced              = false;
        const ScreenBox* ownerName = nullptr;
        if (owner == nullptr)
            ++m_TextStats.noOwner;
        else
        {
            ++(owner->kind == EntityKind::Mob ? m_TextStats.fromMobs : m_TextStats.fromOthers);
            replaced  = ReplacesName(settings, *owner);
            ownerName = NameplateBox(owner->index);
            if (!replaced) m_KeptNow.insert(owner->index);
            uint32_t argb = kWhite;
            if ((fvf & D3DFVF_DIFFUSE) != 0 && stride >= kPretransformedPositionBytes + sizeof(argb))
            {
                std::memcpy(&argb, static_cast<const uint8_t*>(vertices) + kPretransformedPositionBytes, sizeof(argb));
                // FFXI keeps the PS2's color math, where 0x80 is full intensity and the draw doubles it.
                DWORD op = D3DTOP_MODULATE;
                m_Device->GetTextureStageState(0, D3DTSS_COLOROP, &op);
                argb = ShownColor(argb, op == D3DTOP_MODULATE4X ? 4 : op == D3DTOP_MODULATE2X ? 2 : 1);
            }
            m_Glyphs[owner->index].push_back(GlyphDraw{glyph, argb, texture, depth});
        }
        // Only what HeadsUp can draw in its place: its icons go with its names.
        const bool blockable = letter ? blocking.names : blocking.names && blocking.icons;
        if (!blockable || !HideGameGlyph(glyph, letter, replaced, ownerName, m_Last)) return false;
        ++m_TextStats.hidden;
        return true;
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
        const CursorVerdict verdict   = JudgeGameCursor(quad, m_ArrowTexture, m_Last.font, anchors, names,
            owner != nullptr ? std::optional<uint16_t>(owner->index) : std::nullopt);
        if (verdict.learnArrow) m_ArrowTexture = quad.texture;
        uint32_t argb = 0;
        if (verdict.block && (fvf & D3DFVF_DIFFUSE) != 0 && stride >= kPretransformedPositionBytes + sizeof(argb))
        {
            std::memcpy(&argb, static_cast<const uint8_t*>(vertices) + kPretransformedPositionBytes, sizeof(argb));
            if (const auto outOfRange = PickedOutOfRange(argb)) m_PickOutOfRange = *outOfRange;
        }
        return verdict.block;
    }
}
