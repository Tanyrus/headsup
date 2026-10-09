#pragma once

#include "name_frame.h"
#include "native_locate.h"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

struct IDirect3DDevice8;
struct IDirect3DSurface8;

namespace headsup
{
    // Sends every name the game is about to draw through HeadsUp first: the six bytes at the name routine's hook
    // (native_locate.h) become a jump to a gate that calls OnName with the routine's frame, then either runs the bytes
    // it replaced and resumes the routine, or skips to the routine's exit so the game draws nothing.
    // An entity whose names the hook may meet this frame, and whether HeadsUp draws them instead of the game.
    struct NameOwner
    {
        uint32_t actor;
        uint16_t index;
        bool replace;
    };

    struct NameStats
    {
        uint32_t names    = 0; // tracked entities' names the game was about to draw
        uint32_t replaced = 0; // of those, drawn by HeadsUp instead
    };

    class NameHook
    {
    public:
        void SetDevice(IDirect3DDevice8* device);
        ~NameHook() { Stop(); }
        // Only at a render boundary, where the game cannot be inside the routine. Returns why it could not start, or an
        // empty string.
        std::string Start();
        // Puts the game's bytes back if they are still HeadsUp's. Returns why it could not, or an empty string.
        std::string Stop();
        bool Running() const { return m_Site != nullptr; }

        uint32_t ModuleBase() const { return m_ModuleBase; }
        std::span<const uint32_t> CallerReturns() const;

        // At the end of Present, once everything has read the frame's names: forget them, and say whose the next
        // frame's are.
        void NewFrame(std::vector<NameOwner> owners);
        NameStats Stats() const;
        // The frame of the name the game drew for this entity this frame, or nullptr.
        const NameFrame* Drawn(uint16_t index) const;
        // The image and depth buffer this frame's names went to, from the first one; nullptr before any.
        IDirect3DSurface8* SceneTarget() const;
        IDirect3DSurface8* SceneDepth() const;

    private:
        uint8_t* m_Site       = nullptr;
        uint8_t m_Patch[sizeof(kNameHookBytes)] = {};
        uint32_t m_ModuleBase = 0;
    };

    // Sends the game's two target arrows through HeadsUp: the calls that draw them (native_locate.h) go to a gate that
    // notes the arrow's color, then lets the game draw it or skips it while HeadsUp draws its own cursor instead.
    class ArrowHook
    {
    public:
        ~ArrowHook() { Stop(); }
        // Only at a render boundary, where the game cannot be inside the target window's draw. Returns why it could
        // not start, or an empty string.
        std::string Start();
        // Puts the game's calls back if they are still HeadsUp's. Returns why it could not, or an empty string.
        std::string Stop();
        bool Running() const { return m_Calls[0] != nullptr; }

        // Whether the game's arrows are skipped from now on.
        void Hide(bool hide);
        // The color of the arrow the game last drew over a candidate being picked, or 0 since forgotten.
        uint32_t PickedColor() const;
        void ForgetPicked();

    private:
        uint8_t* m_Calls[2]     = {}; // the target's and the candidate's
        uint32_t m_Operands[2]  = {}; // the game's
        uint32_t m_Patched[2]   = {}; // HeadsUp's
    };

    // Puts right the game's guess of its window's client area (native_locate.h's PointerMapping): each guess's
    // GetWindowRect call and GetSystemMetrics load go to HeadsUp, which answers with the client area and no borders
    // while the fix is on, and as Windows would while it is off. It also brings back the pointer the game hid for
    // typing when the mouse moves over an Ashita window, which keeps those moves from the game.
    class PointerFix
    {
    public:
        ~PointerFix() { Stop(); }
        // Only at a render boundary. Returns why it could not start, or an empty string.
        std::string Start();
        // Puts the game's bytes back if they are still HeadsUp's. Returns why it could not, or an empty string.
        std::string Stop();
        bool Running() const { return !m_Patches.empty(); }
        void Enable(bool on);
        // On a mouse move an Ashita window took: shows the game's pointer through its own routine if it is hidden.
        static void RevealUnderWindow();
        // Keys that set off a game command without hiding its pointer (native_locate.h's KeyHide); none keeps the game's
        // behavior.
        void KeepPointerFor(const std::vector<uint8_t>& keys);

    private:
        struct Patch
        {
            uint8_t* at;
            uint8_t game[6], ours[6];
            uint32_t bytes;
        };
        std::vector<Patch> m_Patches;
    };
}
