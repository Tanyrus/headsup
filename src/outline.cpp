#include "outline.h"

#include "outline_math.h"

#include <algorithm>
#include <cstring>
#include <iterator>

namespace headsup
{
    namespace
    {
        constexpr DWORD kWineDeclarationHandles = 0xF0000000; // Wine numbers declaration handles from here
        constexpr uintptr_t kStackScanBytes     = 0x1000;     // the game's draw call chain fits well inside this

        // D3D8 vertex declaration handles have bit 0 set (FVF codes never do).
        bool IsDeclarationHandle(DWORD vs)
        {
            return (vs & 1) != 0 || vs >= kWineDeclarationHandles;
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
        m_TargetSurface    = 0; // a render target recreated at the same address may have a new size
        FinishText();
        m_TextFinished = false;
        ReadBackBufferSize();
    }

    bool OutlineRenderer::TextPending() const
    {
        return !m_TextFinished && !m_Glyphs.empty();
    }

    void OutlineRenderer::FinishText()
    {
        if (m_TextFinished) return;
        m_TextFinished = true;
        m_PlatesLast.clear();
        m_NameColorsLast.clear();
        m_DepthsLast.clear();
        m_GlyphCountsLast.clear();
        std::unordered_map<uint16_t, uint32_t> runs;
        std::vector<ScreenBox> boxes;
        for (const auto& [index, glyphs] : m_Glyphs)
        {
            m_GlyphCountsLast[index] = static_cast<uint32_t>(glyphs.size());
            boxes.clear();
            for (const GlyphDraw& g : glyphs)
                boxes.push_back(g.box);
            const ScreenBox plate = NameplateFromGlyphs(boxes);
            if (!plate.valid) continue;
            m_PlatesLast[index]     = plate;
            m_NameColorsLast[index] = NameColor(glyphs, plate);
            m_DepthsLast[index]     = m_Depths[index];
            runs[index] = PlateFramesInRow(index) + 1;
        }
        m_PlateRuns.swap(runs);
        m_Glyphs.clear();
        m_Depths.clear();
        m_OtherPlatesLast.clear();
        for (auto& [index, glyphs] : m_OtherGlyphs)
        {
            const ScreenBox plate = NameplateFromGlyphs(std::move(glyphs));
            if (plate.valid) m_OtherPlatesLast[index] = plate;
        }
        m_OtherGlyphs.clear();
        m_TextStatsLast = m_TextStats;
        m_TextStats     = TextDrawStats{};
        m_MeshDrawsLast.swap(m_MeshDraws);
        m_MeshDraws.clear();
    }

    void OutlineRenderer::ReadBackBufferSize()
    {
        if (m_Device == nullptr) return;
        IDirect3DSurface8* back = nullptr;
        if (FAILED(m_Device->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &back)) || back == nullptr) return;
        D3DSURFACE_DESC desc{};
        m_BackBuffer = reinterpret_cast<uintptr_t>(back); // the swap chain keeps it alive
        if (SUCCEEDED(back->GetDesc(&desc)))
        {
            m_BackBufferWidth  = static_cast<float>(desc.Width);
            m_BackBufferHeight = static_cast<float>(desc.Height);
        }
        back->Release();
    }

    void OutlineRenderer::SetReplacedNames(const std::vector<uint16_t>& indices)
    {
        m_Replaced.clear();
        m_ReplacedPlates.clear();
        for (const uint16_t index : indices)
        {
            if (const ScreenBox* plate = NameplateBox(index))
            {
                m_Replaced.insert(index);
                m_ReplacedPlates.push_back(*plate);
            }
        }
    }

    const ScreenBox* OutlineRenderer::NameplateBox(uint16_t index) const
    {
        const auto it = m_PlatesLast.find(index);
        return it == m_PlatesLast.end() ? nullptr : &it->second;
    }

    bool OutlineRenderer::SceneCopyStarting()
    {
        if (!TextPending() || m_TargetSurface == 0 || m_TargetSurface == m_BackBuffer) return false;
        IDirect3DSurface8* target = nullptr;
        if (FAILED(m_Device->GetRenderTarget(&target)) || target == nullptr) return false;
        const bool copying = reinterpret_cast<uintptr_t>(target) == m_BackBuffer;
        target->Release();
        return copying;
    }

    bool OutlineRenderer::OnDrawUP(D3DPRIMITIVETYPE type, UINT primCount, const void* vertices, UINT stride,
        const Tracker& tracker, bool collect, bool hide)
    {
        if (!collect || m_Device == nullptr || tracker.Mobs().empty() || m_BackBufferWidth <= 0.0f) return false;
        DWORD vs = 0;
        if (FAILED(m_Device->GetVertexShader(&vs)) || IsDeclarationHandle(vs) || (vs & D3DFVF_POSITION_MASK) != D3DFVF_XYZRHW)
            return false;
        ScreenBox box;
        float depth          = 0.0f;
        const uint32_t count = VertexCount(type, primCount);
        if (!WorldTextBox(vertices, stride, count, box, depth)) return false; // cheap: rejects HUD text
        ++m_TextStats.inScene;

        // Pretransformed coordinates are render-target pixels; nameplates are placed in back-buffer pixels.
        IDirect3DSurface8* target = nullptr;
        if (FAILED(m_Device->GetRenderTarget(&target)) || target == nullptr) return false;
        if (reinterpret_cast<uintptr_t>(target) != m_TargetSurface)
        {
            D3DSURFACE_DESC desc{};
            if (FAILED(target->GetDesc(&desc)) || desc.Width == 0 || desc.Height == 0)
            {
                target->Release();
                return false;
            }
            m_TargetSurface = reinterpret_cast<uintptr_t>(target);
            IDirect3DSurface8* sceneDepth = nullptr;
            m_SceneDepth = SUCCEEDED(m_Device->GetDepthStencilSurface(&sceneDepth)) ? reinterpret_cast<uintptr_t>(sceneDepth) : 0;
            if (sceneDepth != nullptr) sceneDepth->Release();
            m_TargetScaleX  = m_BackBufferWidth / static_cast<float>(desc.Width);
            m_TargetScaleY  = m_BackBufferHeight / static_cast<float>(desc.Height);
        }
        target->Release();
        const ScreenBox glyph = box.Scaled(m_TargetScaleX, m_TargetScaleY);

        const ActorInfo* owner     = FindOwnerOnStack(tracker);
        GlyphOwner kind            = GlyphOwner::None;
        const ScreenBox* ownerName = nullptr;
        if (owner == nullptr)
            ++m_TextStats.noOwner;
        else if (!owner->isMob)
        {
            ++m_TextStats.otherOwner;
            kind = GlyphOwner::Other;
            m_OtherGlyphs[owner->index].push_back(glyph);
            const auto it = m_OtherPlatesLast.find(owner->index);
            if (it != m_OtherPlatesLast.end()) ownerName = &it->second;
        }
        else
        {
            kind = GlyphOwner::Mob;
            if (m_Replaced.count(owner->index) != 0) ownerName = NameplateBox(owner->index);
            ++m_TextStats.owned;
            uint32_t argb = kWhite;
            if ((vs & D3DFVF_DIFFUSE) != 0 && stride >= kPretransformedPositionBytes + sizeof(argb))
            {
                std::memcpy(&argb, static_cast<const uint8_t*>(vertices) + kPretransformedPositionBytes, sizeof(argb));
                // FFXI keeps the PS2's color math, where 0x80 is full intensity and the draw doubles it.
                DWORD op = D3DTOP_MODULATE;
                m_Device->GetTextureStageState(0, D3DTSS_COLOROP, &op);
                argb = ShownColor(argb, op == D3DTOP_MODULATE4X ? 4 : op == D3DTOP_MODULATE2X ? 2 : 1);
            }
            m_Glyphs[owner->index].push_back(GlyphDraw{glyph, argb});
            float& nameDepth = m_Depths[owner->index];
            nameDepth        = std::max(nameDepth, depth);
        }
        if (!hide || !HideGlyph(glyph, kind, ownerName, m_ReplacedPlates)) return false;
        ++m_TextStats.hidden;
        return true;
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
        const uintptr_t end = std::min<uintptr_t>(reinterpret_cast<uintptr_t>(tib->StackBase), sp + kStackScanBytes);
        return FindOwner(reinterpret_cast<const uint32_t*>(sp & ~uintptr_t{3}), reinterpret_cast<const uint32_t*>(end & ~uintptr_t{3}), tracker);
    }

    bool OutlineRenderer::OnDrawIndexed(D3DPRIMITIVETYPE type, UINT minIndex, UINT numVertices, UINT startIndex,
        UINT primCount, const Tracker& tracker, const Settings& settings)
    {
        if (m_InDraw || m_Device == nullptr) return false;
        const bool outlines = settings.enabled && tracker.OutlinedCount() > 0;
        const bool bodies   = NameplatesOn(settings) && !tracker.Mobs().empty(); // nameplates need to know who was drawn
        if (!outlines && !bodies) return false;
        if (!IsCharacterModelDraw()) return false;
        const ActorInfo* owner = FindOwnerOnStack(tracker);
        if (owner == nullptr) return false;
        if (owner->isMob) ++m_MeshDraws[owner->index];
        if (!outlines || !owner->outline) return false;
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

        // The mesh as the game drew it, also marking its visible pixels with this mob's stencil value.
        m_Device->SetRenderState(D3DRS_STENCILENABLE, TRUE);
        m_Device->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_ALWAYS);
        m_Device->SetRenderState(D3DRS_STENCILREF, owner->stencilRef);
        m_Device->SetRenderState(D3DRS_STENCILMASK, 0xFF);
        m_Device->SetRenderState(D3DRS_STENCILWRITEMASK, 0xFF);
        m_Device->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_REPLACE);
        m_Device->SetRenderState(D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP);
        m_Device->SetRenderState(D3DRS_STENCILZFAIL, D3DSTENCILOP_KEEP);
        draw();

        // Solid-colour copies shifted around the silhouette, drawn only where the stencil is not this mob.
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

        m_Device->SetTransform(D3DTS_PROJECTION, &projection);
        for (size_t i = 0; i < std::size(kSavedStates); ++i)
            m_Device->SetRenderState(kSavedStates[i], states[i]);
        for (size_t i = 0; i < std::size(kSavedStages); ++i)
            m_Device->SetTextureStageState(kSavedStages[i].stage, kSavedStages[i].type, stages[i]);
        ++m_Meshes;
        return true;
    }
}
