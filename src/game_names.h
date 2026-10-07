#pragma once

#include "Ashita.h"
#include "d3d_util.h"
#include "game_cursor.h"
#include "game_glyphs.h"
#include "pose.h"
#include "settings.h"
#include "tracker.h"

#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace headsup
{
    // What HeadsUp learns from the game drawing its scene: each name's box, color and depth (FrameNames), the scene
    // image and its camera, which bodies were drawn, and which of the game's glyphs and cursors to block.
    class GameNames
    {
    public:
        void SetDevice(IDirect3DDevice8* device) { m_Device = device; }

        // Called at Present: finishes this frame's names if nothing did, then starts the next frame.
        void NewFrame();
        // The zone changed: the camera seen before means nothing in the new one.
        void ForgetCamera() { m_HaveCamera = false; }

        // Turns this frame's glyphs into names (ReadFrameNames), once: when the game has drawn its names, before ours.
        void FinishText();
        // Whether this frame has glyphs of the game's names not yet turned into names.
        bool TextPending() const { return !m_TextFinished && !m_Glyphs.empty(); }
        // Whether this frame has seen any glyph of the game's names.
        bool NamesThisFrame() const { return m_NamesThisFrame; }

        // What may be blocked: nothing HeadsUp cannot draw in its place, once its name or icon textures have failed.
        struct Blocking
        {
            bool names = true;
            bool icons = true;
        };
        // Measures the game's in-scene glyphs (and the camera, from an entity's fixed-function draw) while nameplates are
        // on. Returns true, for the caller to block it, for a glyph HideGameGlyph hides, decided in the frame it is drawn.
        bool OnDrawUP(D3DPRIMITIVETYPE type, UINT primCount, const void* vertices, UINT stride, const Tracker& tracker,
            const Settings& settings, Blocking blocking);
        // Whether a DrawPrimitiveUP is the game's target cursor where HeadsUp draws its own (names: this frame's cursors),
        // read against the target window's anchors (JudgeGameCursor).
        bool IsGameCursorDraw(D3DPRIMITIVETYPE type, UINT primCount, const void* vertices, UINT stride, const Tracker& tracker,
            const std::vector<CursorName>& names, bool picking, ITarget* target);
        // A character body was drawn for this entity.
        void CountMesh(uint16_t index) { ++m_MeshDraws[index]; }

        // True when the game is about to copy its scene image, holding this frame's names, to the back buffer: the moment
        // to draw into that image (see plugin.cpp).
        bool SceneCopyStarting();
        // The image this frame's names were drawn into, its depth surface, and its pixels to the back buffer's.
        IDirect3DSurface8* SceneTarget() const { return reinterpret_cast<IDirect3DSurface8*>(m_Scene.surface); }
        IDirect3DSurface8* SceneDepth() const { return reinterpret_cast<IDirect3DSurface8*>(m_SceneDepth); }
        float TargetScaleX() const { return m_Scene.x; }
        float TargetScaleY() const { return m_Scene.y; }
        float BackBufferWidth() const { return m_BackBufferWidth; }
        float BackBufferHeight() const { return m_BackBufferHeight; }
        // The scene's camera, this frame's or the last one seen in this zone; nullptr before any.
        const Camera* SceneCamera() const { return m_HaveCamera ? &m_Camera : nullptr; }

        // The frame that just ended, or this one once FinishText has run: an entity's name in back-buffer pixels
        // (nullptr without one), with the game's icons beside it, and its depth, color and run.
        const ScreenBox* NameplateBox(uint16_t index) const { return Find(m_Last.plates, index); }
        const ScreenBox* WholeNameplate(uint16_t index) const { return Find(m_Last.wholes, index); }
        float NameplateDepth(uint16_t index) const { return Value(m_Last.depths, index, 0.0f); }
        uint32_t NameplateColor(uint16_t index) const { return Value(m_Last.colors, index, kWhite); }
        uint32_t PlateFramesInRow(uint16_t index) const { return Value(m_Last.runs, index, 0u); }
        // The name's letter height over its last few frames, steady through a frame or two of stray glyphs.
        float NameSize(uint16_t index) const { return Value(m_Last.sizes, index, 0.0f); }
        uint32_t GlyphsLastFrame(uint16_t index) const { return Value(m_Last.glyphCounts, index, 0u); }
        // The texture the names' letters were drawn from; 0 before any.
        uintptr_t FontTexture() const { return m_Last.font; }
        // Character body draws of an entity: 0 when the game did not draw its body.
        uint32_t MeshDraws(uint16_t index) const { return Value(m_MeshDrawsLast, index, 0u); }

        // Counts of the game's in-scene glyph draws, for /hu debug and the Debug page.
        struct TextDrawStats
        {
            uint32_t inScene = 0, fromMobs = 0, fromOthers = 0, noOwner = 0, hidden = 0;
        };
        const TextDrawStats& TextStatsLastFrame() const { return m_TextStatsLast; }

    private:
        template <typename Map>
        static const typename Map::mapped_type* Find(const Map& map, uint16_t index)
        {
            const auto it = map.find(index);
            return it == map.end() ? nullptr : &it->second;
        }
        template <typename Map, typename T>
        static T Value(const Map& map, uint16_t index, T fallback)
        {
            const auto* found = Find(map, index);
            return found != nullptr ? *found : fallback;
        }

        void CaptureCamera(const Tracker& tracker);
        void ReadBackBufferSize();

        IDirect3DDevice8* m_Device = nullptr;
        float m_BackBufferWidth    = 0.0f;
        float m_BackBufferHeight   = 0.0f;
        uintptr_t m_BackBuffer     = 0;

        // This frame, reset by NewFrame.
        std::unordered_map<uint16_t, std::vector<GlyphDraw>> m_Glyphs; // by entity index
        std::unordered_set<uint16_t> m_KeptNow;                        // entities whose names stay the game's
        std::unordered_map<uint16_t, uint32_t> m_MeshDraws;
        TextDrawStats m_TextStats;
        bool m_TextFinished   = false;
        bool m_NamesThisFrame = false;
        bool m_CameraSeen     = false;

        // Snapshots by FinishText.
        FrameNames m_Last;
        std::unordered_map<uint16_t, uint32_t> m_MeshDrawsLast;
        TextDrawStats m_TextStatsLast;

        uintptr_t m_NamesImage   = 0; // the render target names' letters go to (InNamesImage)
        TargetScale m_Scene;          // that image's scale
        uintptr_t m_SceneDepth   = 0; // and its depth surface
        TargetScale m_Ui;             // the UI image's scale
        uintptr_t m_ArrowTexture = 0; // the game's target arrows, once seen
        Camera m_Camera{};
        bool m_HaveCamera = false;
    };
}
