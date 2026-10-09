#pragma once

#include "Ashita.h"
#include "game_cursor.h"
#include "game_glyphs.h"
#include "pose.h"
#include "settings.h"
#include "tracker.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace headsup
{
    // What HeadsUp learns from the game's draws besides its names, which come from NameHook: the scene's image and
    // camera, which bodies were drawn, the font of the names, and the game's own target cursor.
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

        // The image and depth buffer the game's name routine drew into this frame (NameHook): the names themselves may
        // all be HeadsUp's, leaving no letters to find them by.
        void UseScene(IDirect3DSurface8* target, IDirect3DSurface8* depth);

        // The scene's camera, from the fixed-function draws characters own.
        void OnDrawUP(const Tracker& tracker, const Settings& settings);
        // The game draws its arrow after our nameplates, so this is the frame before's.
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

        uint32_t MeshDraws(uint16_t index) const;
        uint32_t FramesSinceMesh(uint16_t index) const;

    private:
        struct TargetScale
        {
            uintptr_t surface = 0; // 0 to read again: a target made at the same address may differ
            float x = 1.0f, y = 1.0f;
            bool Read(IDirect3DSurface8* target, float backBufferWidth, float backBufferHeight);
        };

        bool TextPending() const { return !m_TextFinished && m_NamesThisFrame; }
        void CaptureCamera(const Tracker& tracker);
        void ReadBackBufferSize();

        IDirect3DDevice8* m_Device = nullptr;
        float m_BackBufferWidth    = 0.0f;
        float m_BackBufferHeight   = 0.0f;
        uintptr_t m_BackBuffer     = 0;

        std::unordered_map<uint16_t, uint32_t> m_MeshDraws;
        std::unordered_map<uint16_t, uint32_t> m_MeshDrawsLast;
        std::unordered_map<uint16_t, uint32_t> m_LastMeshFrame;
        uint32_t m_Frame      = 0;
        bool m_TextFinished   = false;
        bool m_NamesThisFrame = false;
        bool m_CameraSeen     = false;

        TargetScale m_Scene;
        uintptr_t m_SceneDepth   = 0;
        Camera m_Camera{};
        bool m_HaveCamera = false;
    };
}
