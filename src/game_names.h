#pragma once

#include "Ashita.h"
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
    class GameNames
    {
    public:
        void SetDevice(IDirect3DDevice8* device) { m_Device = device; }

        void NewFrame();
        // A camera seen in another zone means nothing in this one.
        void ForgetCamera() { m_HaveCamera = false; }

        void FinishText();
        // Unlike TextPending, still true after FinishText: EndScene draws on top if drawing into the scene failed.
        bool NamesThisFrame() const { return m_NamesThisFrame; }

        struct Blocking
        {
            bool names = true;
            bool icons = true;
        };
        bool OnDrawUP(D3DPRIMITIVETYPE type, UINT primCount, const void* vertices, UINT stride, const Tracker& tracker,
            const Settings& settings, Blocking blocking);
        bool IsGameCursorDraw(D3DPRIMITIVETYPE type, UINT primCount, const void* vertices, UINT stride, const Tracker& tracker,
            const std::vector<CursorName>& names, bool picking, ITarget* target);
        // The game draws its arrow after our nameplates, so this is the frame before's.
        bool PickOutOfRange() const { return m_PickOutOfRange; }
        void SetEnlarged(uint16_t index) { m_Enlarged = index; }
        void ForgetPickRange() { m_PickOutOfRange = false; }
        void CountMesh(uint16_t index) { ++m_MeshDraws[index]; }

        // The game is about to copy its scene image, names included, to the back buffer: the last moment to draw into it.
        bool SceneCopyStarting();
        IDirect3DSurface8* SceneTarget() const { return reinterpret_cast<IDirect3DSurface8*>(m_Scene.surface); }
        IDirect3DSurface8* SceneDepth() const { return reinterpret_cast<IDirect3DSurface8*>(m_SceneDepth); }
        float TargetScaleX() const { return m_Scene.x; }
        float TargetScaleY() const { return m_Scene.y; }
        float BackBufferWidth() const { return m_BackBufferWidth; }
        float BackBufferHeight() const { return m_BackBufferHeight; }
        const Camera* SceneCamera() const { return m_HaveCamera ? &m_Camera : nullptr; }

        // The frame that just ended, or this one once FinishText has run.
        const ScreenBox* NameplateBox(uint16_t index) const { return Find(m_Last.plates, index); }
        const ScreenBox* WholeNameplate(uint16_t index) const { return Find(m_Last.wholes, index); }
        float NameplateDepth(uint16_t index) const { return Value(m_Last.depths, index, 0.0f); }
        uint32_t NameplateColor(uint16_t index) const { return Value(m_Last.colors, index, kWhite); }
        uint32_t PlateFramesInRow(uint16_t index) const { return Value(m_Last.framesInRow, index, 0u); }
        float NameSize(uint16_t index) const { return Value(m_Last.sizes, index, 0.0f); }
        uint32_t GlyphsLastFrame(uint16_t index) const { return Value(m_Last.glyphCounts, index, 0u); }
        uintptr_t FontTexture() const { return m_Last.font; }
        uint32_t MeshDraws(uint16_t index) const { return Value(m_MeshDrawsLast, index, 0u); }
        uint32_t FramesSinceMesh(uint16_t index) const;

        struct TextDrawStats
        {
            uint32_t inScene = 0, fromMobs = 0, fromOthers = 0, noOwner = 0, hidden = 0;
        };
        const TextDrawStats& TextStatsLastFrame() const { return m_TextStatsLast; }

    private:
        struct TargetScale
        {
            uintptr_t surface = 0; // 0 to read again: a target made at the same address may differ
            float x = 1.0f, y = 1.0f;
            bool Read(IDirect3DSurface8* target, float backBufferWidth, float backBufferHeight);
        };

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

        bool TextPending() const { return !m_TextFinished && !m_Glyphs.empty(); }
        void CaptureCamera(const Tracker& tracker);
        void ReadBackBufferSize();

        IDirect3DDevice8* m_Device = nullptr;
        float m_BackBufferWidth    = 0.0f;
        float m_BackBufferHeight   = 0.0f;
        uintptr_t m_BackBuffer     = 0;

        std::unordered_map<uint16_t, std::vector<GlyphDraw>> m_Glyphs;
        TextureUse m_TextureUse;
        std::unordered_set<uint16_t> m_KeptNow;
        std::unordered_map<uint16_t, uint32_t> m_MeshDraws;
        TextDrawStats m_TextStats;
        bool m_TextFinished   = false;
        bool m_NamesThisFrame = false;
        bool m_CameraSeen     = false;

        FrameNames m_Last;
        std::unordered_map<uint16_t, uint32_t> m_MeshDrawsLast;
        std::unordered_map<uint16_t, uint32_t> m_LastMeshFrame;
        uint32_t m_Frame = 0;
        TextDrawStats m_TextStatsLast;

        uintptr_t m_NamesImage   = 0;
        TargetScale m_Scene;
        uintptr_t m_SceneDepth   = 0;
        TargetScale m_Ui;
        uintptr_t m_ArrowTexture = 0;
        bool m_PickOutOfRange    = false;
        uint16_t m_Enlarged      = 0;
        Camera m_Camera{};
        bool m_HaveCamera = false;
    };
}
