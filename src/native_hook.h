#pragma once

#include "code_patch.h"
#include "friend_list.h"
#include "name_frame.h"
#include "native_locate.h"
#include "plate_settings.h"
#include "strip_text.h"
#include "weather.h"
#include "window_views.h"

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

struct IDirect3DDevice8;
struct IDirect3DSurface8;

namespace headsup
{
    constexpr const char* kGameModule = "FFXiMain.dll";
    // The folder a loaded module was loaded from, without its last separator; empty when it is not loaded.
    std::string ModuleFolder(const char* module);

    // Sends every name the game is about to draw through HeadsUp first: the six bytes at the name routine's hook
    // (native_locate.h) become a jump to a gate that calls OnName with the routine's frame, then either runs the bytes
    // it replaced and resumes the routine, or skips to the routine's exit so the game draws nothing.
    // An entity whose names the hook may meet this frame, and what becomes of them: kept, hidden or replaced.
    struct NameOwner
    {
        uint32_t actor;
        uint16_t index;
        HookAction action;
    };

    struct NameStats
    {
        uint32_t names    = 0; // tracked entities' names the game was about to draw
        uint32_t replaced = 0; // of those, drawn by HeadsUp instead
        uint32_t hidden   = 0; // of those, drawn by no one
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
        bool Running() const { return m_Patch.at != nullptr; }

        uint32_t ModuleBase() const { return m_ModuleBase; }
        std::span<const uint32_t> CallerReturns() const;

        // At the end of Present, once everything has read the frame's names: forget them, and say whose the next
        // frame's are.
        void NewFrame(std::vector<NameOwner> owners);
        // From now to the next NewFrame every name the game draws goes, whatever its owner's action.
        void HideAll();
        NameStats Stats() const;
        // The frame of the name the game drew for this entity this frame, or nullptr.
        const NameFrame* Drawn(uint16_t index) const;
        // The image and depth buffer this frame's names went to, from the first one; nullptr before any.
        IDirect3DSurface8* SceneTarget() const;
        IDirect3DSurface8* SceneDepth() const;

    private:
        CodePatch m_Patch;
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
        bool Running() const { return m_Calls[0].at != nullptr; }

        // Whether the game's arrows are skipped from now on.
        void Hide(bool hide);
        // The color of the arrow the game last drew over a candidate being picked, or 0 since forgotten.
        uint32_t PickedColor() const;
        void ForgetPicked();
        // The color the game would give that arrow now, asked of its own routine: the draw that asks is skipped while
        // HudBeGone hides the target window. 0 when the routine was not found or there is nobody to ask about.
        uint32_t CandidateColor(const void* window, uintptr_t yourActor, uintptr_t candidateActor) const;

    private:
        std::array<CodePatch, 2> m_Calls; // the target's and the candidate's
    };

    // Skips the game's help bar and connection readout while asked: the draw loop's call to the menu window draw
    // (native_locate.h) goes to HeadsUp, which passes every window but those hidden on to the game.
    // How the game's windows are placed this frame: the panel's band, and the settings for the help's mode.
    struct WindowPlacing
    {
        HelpBand band;
        int placedUnder   = 0;
        bool shift        = false; // Shift Vanilla UI
        bool moveGameHelp = false; // the game's strip goes right under the panel, and the windows under it
    };

    class WindowHook
    {
    public:
        ~WindowHook() { Stop(); }
        // Only at a render boundary. Returns why it could not start, or an empty string.
        std::string Start();
        // Puts the game's call back if it is still HeadsUp's. Returns why it could not, or an empty string.
        std::string Stop();
        bool Running() const { return m_Call.at != nullptr; }
        // Which of the game's windows are skipped from now on.
        void Hide(HiddenWindows hidden);
        // The windows, by short name, whose draws are dropped from now on (Base Game UI's Hide).
        void HideWindows(const std::vector<std::string>& names);
        // While the game draws one of those, every draw call it makes is one to drop.
        static bool Silent();
        // Where those drawn in the last whole frame are, in the game's menu pixels: clicks there are kept from it.
        static std::span<const WindowRect> HiddenRects();
        // Once a frame, before the game draws its windows: Stop puts back those that drew in the last two.
        void NewFrame();
        // While placing is set, the game's windows are placed as they draw: at the first, the frame's windows are read
        // from the game's own list and its view picked (window_views.h); each that hangs from the top then moves by
        // the view's shift and its own offset, the strip by its own rule, and every window that opens is noted.
        void Place(std::optional<WindowPlacing> placing);
        // The player's shifts for the views of the help's mode now.
        void Shift(const std::vector<ViewShift>& shifts);
        // The places the player put windows at: such a window goes there, and no view's shift moves it.
        void Pin(const std::vector<WindowPlace>& places);
        // Where each window seen since HeadsUp loaded last stood, by short name.
        std::vector<WindowPlace> Places() const;
        // The line the windows went under in the last frame placed, in the game's menu pixels; 0 before one.
        int WindowsBand() const;
        // The windows that drew since the last call, by short name, sorted: those open now.
        std::vector<std::string> TakeOpen();
        // The windows noted since HeadsUp loaded, by short name, in the order they came.
        std::vector<std::string> SeenWindows() const;
        size_t SeenCount() const;
        // Why windows cannot be moved, once, after a Start that could not find the game's move.
        std::string TakeMoveProblem() { return std::exchange(m_MoveProblem, std::string()); }

    private:
        CodePatch m_Call;
        CodePatch m_LabelCall; // Wide Scan's grid label's ask for the strip title's place, when it was found
        CodePatch m_CountCall; // an item list's count's ask for it, when it was found
        CodePatch m_MoveStart; // MCD::MoveTo's start, so a window the game moves goes where HeadsUp would put it
        CodePatch m_BuildCall; // the catalog's window build, so where a new window goes is known before it draws
        std::string m_MoveProblem;
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
        std::vector<CodePatch> m_Patches;
    };

    // One strip window's text object as read, or why not.
    struct StripRead
    {
        uint32_t owner = 0;
        uint32_t at    = 0;          // the text object's address, once read
        std::vector<uint8_t> object; // kStripObjectBytes, empty when it could not be read
        std::string why;             // empty when read
    };

    // The game's top-strip text (native_locate.h's LocateStripText), read on the game's thread at Present.
    class StripTextReader
    {
    public:
        // Finds the slots and offsets: "" when found, else why not.
        std::string Find();
        // Each check fails closed: no slot, a 0 owner (the title screen, character select, a teardown), an owner
        // whose window record does not name the window, or memory that cannot be read. Every check, every call.
        StripRead Read(StripWindow window) const;
        // Each frame: the window's text object into into, false when hidden. Memory is checked only where the chain
        // to it changed (StripWindowReader).
        bool Read(StripWindow window, StripObject& into);

    private:
        StripSites m_Sites;
        std::array<StripWindowReader, 2> m_Readers; // by StripWindow
    };
    // The client's connection counters (LocateNetStats), read on the game's thread.
    // The Window Type the game is set to, by the client's own config getter, found once.
    class WindowTypeReader
    {
    public:
        // "" when found, else why not.
        std::string Find();
        // 0 to 7; nothing before Find succeeded or when the game answers no type.
        std::optional<int> Read() const;

    private:
        int32_t(__cdecl* m_Get)(int32_t) = nullptr;
    };

    // The zone's weather as the client keeps it, found once.
    class WeatherReader
    {
    public:
        // "" when found, else why not.
        std::string Find();
        // Nothing before Find succeeded or when what is there is no weather's id.
        std::optional<Weather> Read() const;

    private:
        uint32_t m_Global = 0;
    };

    // The friends of the game's own friend list (friend_list.h), read where the client keeps it.
    class FriendListReader
    {
    public:
        // The characters its friends play, by server id in ascending order; none while the list is not found. It
        // is looked for on the first reads, as the client fills it once you are signed on.
        std::vector<uint32_t> Read();

    private:
        const uint8_t* m_First = nullptr;
        int m_Searches         = 0;
    };

    class NetStatsReader
    {
    public:
        // Finds where the client keeps them: "" when found, else why not.
        std::string Find();
        // Nothing before Find succeeded, while the client has no session (the title screen), or when what is there
        // cannot be read or cannot be the counters.
        std::optional<NetCounters> Read() const;

    private:
        uint32_t m_Global = 0;
    };
}
