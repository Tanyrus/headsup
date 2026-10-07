#pragma once

#include "Ashita.h"
#include "nameplate.h"
#include "outline_math.h"
#include "settings.h"
#include "tracker.h"

#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace headsup
{
    // Outlines mob meshes in DrawIndexedPrimitive; measures, or hides, the game's mob name letters in DrawPrimitiveUP.
    class OutlineRenderer
    {
    public:
        void SetDevice(IDirect3DDevice8* device) { m_Device = device; }

        // Called at Present: starts the next frame, finishing this frame's nameplates if EndScene did not.
        void NewFrame();

        // Turns this frame's collected glyphs into nameplate boxes (and snapshots per-mob mesh counts), once per frame:
        // when the game has drawn its names, before our nameplates are laid out. NewFrame calls it when nothing else did.
        void FinishText();
        // True when this frame has collected nameplate glyphs that FinishText has not yet turned into boxes.
        bool TextPending() const;
        // True when glyphs are pending and the game is about to draw to the back buffer after drawing them into its
        // scene image: the moment to draw into that image before it is copied (see plugin.cpp).
        bool SceneCopyStarting();
        // The render target and depth surface this frame's nameplate glyphs were drawn into.
        IDirect3DSurface8* SceneTarget() const { return reinterpret_cast<IDirect3DSurface8*>(m_TargetSurface); }
        IDirect3DSurface8* SceneDepth() const { return reinterpret_cast<IDirect3DSurface8*>(m_SceneDepth); }
        // Scene-image pixels to back-buffer pixels.
        float TargetScaleX() const { return m_TargetScaleX; }
        float TargetScaleY() const { return m_TargetScaleY; }
        // How many glyphs the game drew for a mob in the frame that just ended, for /hu debug.
        uint32_t GlyphsLastFrame(uint16_t index) const
        {
            const auto it = m_GlyphCountsLast.find(index);
            return it == m_GlyphCountsLast.end() ? 0 : it->second;
        }
        // The depth the game drew a mob's name at in the frame that just ended; 0 without a nameplate.
        float NameplateDepth(uint16_t index) const
        {
            const auto it = m_DepthsLast.find(index);
            return it == m_DepthsLast.end() ? 0.0f : it->second;
        }

        // The mobs whose names HeadsUp drew over this frame's boxes: their letters are hidden in the next frame.
        void SetReplacedNames(const std::vector<uint16_t>& indices);

        // Returns true when it drew the mesh itself; the caller must then block the original call.
        bool OnDrawIndexed(D3DPRIMITIVETYPE type, UINT minIndex, UINT numVertices, UINT startIndex, UINT primCount,
            const Tracker& tracker, const Settings& settings);

        // Records the screen box and color of a mob's nameplate letters from the game's pretransformed text draws (every
        // mob, when collect is on). With hide on, returns true for a letter of a mob's nameplate (HideGlyph) so the
        // caller blocks it; returns false for everything else.
        bool OnDrawUP(D3DPRIMITIVETYPE type, UINT primCount, const void* vertices, UINT stride, const Tracker& tracker,
            bool collect, bool hide);

        // An entity's nameplate box in the frame that just ended, in back-buffer pixels; nullptr when it had none.
        const ScreenBox* NameplateBox(uint16_t index) const;
        float BackBufferWidth() const { return m_BackBufferWidth; }

        // Per-frame counts of in-scene pretransformed text draws, for /hu debug.
        struct TextDrawStats
        {
            uint32_t inScene = 0, owned = 0, noOwner = 0, otherOwner = 0, hidden = 0;
        };
        const TextDrawStats& TextStatsLastFrame() const { return m_TextStatsLast; }
        // Mesh draws of a mob in the frame that just ended: 0 when the game did not draw its body.
        uint32_t MeshDraws(uint16_t index) const
        {
            const auto it = m_MeshDrawsLast.find(index);
            return it == m_MeshDrawsLast.end() ? 0 : it->second;
        }
        // The color the game draws this mob's name in (from the frame that just ended); white when unknown.
        uint32_t NameplateColor(uint16_t index) const
        {
            const auto it = m_NameColorsLast.find(index);
            return it == m_NameColorsLast.end() ? kWhite : it->second;
        }
        // How many frames in a row, up to the one that just ended, this entity's nameplate was seen.
        uint32_t PlateFramesInRow(uint16_t index) const
        {
            const auto it = m_PlateRuns.find(index);
            return it == m_PlateRuns.end() ? 0 : it->second;
        }
        float BackBufferHeight() const { return m_BackBufferHeight; }

        uint32_t MeshesLastFrame() const { return m_MeshesLast; }
        bool StencilAvailable() const { return m_StencilAvailable; }
        // True once, the first time a mob could not be outlined because its depth buffer has no stencil bits.
        bool TakeStencilWarning();

    private:
        bool IsCharacterModelDraw();
        bool BoundSurfaceHasStencil();
        const ActorInfo* FindOwnerOnStack(const Tracker& tracker);
        void ReadBackBufferSize();

        IDirect3DDevice8* m_Device   = nullptr;
        bool m_InDraw                = false;
        bool m_ClearedThisFrame      = false;
        StencilCheck m_StencilCheck;
        bool m_StencilWarned         = false;
        bool m_StencilAvailable      = true;
        bool m_StencilWarningPending = false;
        uint32_t m_Meshes            = 0;
        uint32_t m_MeshesLast        = 0;
        std::unordered_map<uint16_t, std::vector<GlyphDraw>> m_Glyphs; // this frame's glyphs, by entity index
        std::unordered_map<uint16_t, uint32_t> m_NameColorsLast;       // the frame that just ended
        std::unordered_map<uint16_t, std::vector<ScreenBox>> m_OtherGlyphs; // players' and NPCs' letters this frame
        std::unordered_map<uint16_t, ScreenBox> m_OtherPlatesLast;          // their names in the frame that just ended
        std::unordered_set<uint16_t> m_Replaced;                            // see SetReplacedNames
        std::vector<ScreenBox> m_ReplacedPlates;
        uintptr_t m_TargetSurface = 0;                                 // render target the scale was read for
        uintptr_t m_SceneDepth    = 0;                                 // its depth surface
        uintptr_t m_BackBuffer    = 0;
        std::unordered_map<uint16_t, float> m_Depths, m_DepthsLast;    // farthest glyph depth by entity index
        std::unordered_map<uint16_t, uint32_t> m_GlyphCountsLast;
        float m_TargetScaleX = 1.0f, m_TargetScaleY = 1.0f;            // render-target to back-buffer pixels
        std::unordered_map<uint16_t, ScreenBox> m_PlatesLast;          // the frame that just ended
        bool m_TextFinished = false;
        std::unordered_map<uint16_t, uint32_t> m_PlateRuns; // frames in a row each entity's nameplate was seen
        TextDrawStats m_TextStats;
        TextDrawStats m_TextStatsLast;
        std::unordered_map<uint16_t, uint32_t> m_MeshDraws;
        std::unordered_map<uint16_t, uint32_t> m_MeshDrawsLast;
        float m_BackBufferWidth  = 0.0f;
        float m_BackBufferHeight = 0.0f;
    };
}
