#pragma once

#include "Ashita.h"
#include "nameplate.h"
#include "outline_math.h"
#include "settings.h"
#include "tracker.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace aggroglow
{
    // Outlines mob meshes from inside the plugin's DrawIndexedPrimitive callback (spec: Rendering).
    class OutlineRenderer
    {
    public:
        void SetDevice(IDirect3DDevice8* device) { m_Device = device; }

        // Called at Present: next frame clears stencil again; the mesh counter rolls over; the nameplate boxes
        // collected this frame become the ones NameplateBox returns.
        void NewFrame();

        // Turns this frame's collected glyphs into nameplate boxes (and snapshots per-mob mesh counts). Call once the
        // frame's nameplates are drawn, i.e. at the back-buffer EndScene; NewFrame calls it when nothing else did.
        void FinishText();
        // True when this frame has collected nameplate glyphs that FinishText has not yet turned into boxes.
        bool TextPending() const;

        // Returns true when it drew the mesh itself; the caller must then block the original call.
        bool OnDrawIndexed(D3DPRIMITIVETYPE type, UINT minIndex, UINT numVertices, UINT startIndex, UINT primCount,
            const Tracker& tracker, const Settings& settings);

        // Records the screen box of an outlined mob's nameplate from the game's pretransformed text draws. Draws and
        // blocks nothing.
        void OnDrawUP(D3DPRIMITIVETYPE type, UINT primCount, const void* vertices, UINT stride, const Tracker& tracker,
            bool collect);

        // An entity's nameplate box in the frame that just ended, in back-buffer pixels; nullptr when it had none.
        const ScreenBox* NameplateBox(uint16_t index) const;
        float BackBufferWidth() const { return m_BackBufferWidth; }

        // Per-frame counts of in-scene pretransformed text draws, for /ag debug.
        struct TextDrawStats
        {
            uint32_t inScene = 0, owned = 0, noOwner = 0, otherOwner = 0;
        };
        const TextDrawStats& TextStatsLastFrame() const { return m_TextStatsLast; }
        // Mesh draws of an outlined mob in the frame that just ended: 0 when the game did not draw its body.
        uint32_t MeshDraws(uint16_t index) const
        {
            const auto it = m_MeshDrawsLast.find(index);
            return it == m_MeshDrawsLast.end() ? 0 : it->second;
        }
        const ScreenBox* RawNameplateBox(uint16_t index) const
        {
            const auto it = m_RawPlatesLast.find(index);
            return it == m_RawPlatesLast.end() ? nullptr : &it->second;
        }
        // How many frames in a row, up to the one that just ended, this entity's nameplate was seen.
        uint32_t PlateFramesInRow(uint16_t index) const
        {
            const auto it = m_PlateRuns.find(index);
            return it == m_PlateRuns.end() ? 0 : it->second;
        }
        uint32_t PlateDraws(uint16_t index) const
        {
            const auto it = m_PlateDrawsLast.find(index);
            return it == m_PlateDrawsLast.end() ? 0 : it->second;
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
        std::unordered_map<uint16_t, std::vector<ScreenBox>> m_Glyphs; // this frame's glyph boxes, by entity index
        std::unordered_map<uint16_t, ScreenBox> m_PlatesLast;          // the frame that just ended
        std::unordered_map<uint16_t, ScreenBox> m_RawPlatesLast;       // diagnostics: union of every attributed glyph
        bool m_TextFinished = false;
        std::unordered_map<uint16_t, uint32_t> m_PlateRuns; // frames in a row each entity's nameplate was seen
        TextDrawStats m_TextStats;
        TextDrawStats m_TextStatsLast;
        std::unordered_map<uint16_t, uint32_t> m_MeshDraws;
        std::unordered_map<uint16_t, uint32_t> m_MeshDrawsLast;
        std::unordered_map<uint16_t, uint32_t> m_PlateDraws;
        std::unordered_map<uint16_t, uint32_t> m_PlateDrawsLast;
        float m_BackBufferWidth  = 0.0f;
        float m_BackBufferHeight = 0.0f;
    };
}
