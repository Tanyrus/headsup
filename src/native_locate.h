#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace headsup
{
    // A section of the client's image as loaded: the code exists only in memory, as FFXiMain.dll's .text is unpacked
    // at load.
    struct ImageSection
    {
        uint32_t rva;
        const uint8_t* bytes;
        uint32_t size;
    };

    // The game's name routine overwritten at hook, a whole number of instructions the gate runs again on its way to
    // resume.
    constexpr uint8_t kNameHookBytes[] = {0x33, 0xD2, 0x33, 0xED, 0x33, 0xF6}; // xor edx,edx; xor ebp,ebp; xor esi,esi

    enum class LocateProblem : uint8_t
    {
        None,
        NoIconTables,
        NoTableReads,
        AmbiguousTableReads,
        UnexpectedPrologue,
        PatchSiteChanged,
        UnexpectedExit,
        NoCallers,
        NoEntityCaller,
        NoArrowDraws,
        AmbiguousArrowDraws,
        ArrowDrawsDiffer,
        NoRangeColor,
        AmbiguousRangeColor,
        NoPointerMapping,
        AmbiguousPointerMapping,
        PointerMappingDiffers,
        NoPointerShow,
        AmbiguousPointerShow,
        PointerShowChanged,
        NoKeyHide,
        AmbiguousKeyHide,
        KeyHideDiffers,
        NoWindowDraw,
        AmbiguousWindowDraw,
        WindowDrawChanged,
        NoStripText,
        AmbiguousStripText,
        StripTextChanged,
        NoNetStats,
        AmbiguousNetStats,
        NetStatsChanged,
        NoWindowMove,
        AmbiguousWindowMove,
        NoWeather,
        AmbiguousWeather,
    };
    const char* Describe(LocateProblem problem);

    // RVAs.
    struct NameRoutine
    {
        LocateProblem problem = LocateProblem::None;
        uint32_t entry = 0, hook = 0, resume = 0, exit = 0;
        std::vector<uint32_t> callerReturns;
        uint32_t entityCaller = 0; // the one of those that entities' names come back to
    };

    // Found by the two icon tables it reads (from .rdata, which is in the file) rather than by its own bytes, so it
    // does not depend on where the client was loaded or on a committed copy of the game's code.
    NameRoutine LocateNameRoutine(uint32_t moduleBase, const ImageSection& text, const ImageSection& rdata);

    // RVAs of the calls at the end of the target window's draw that draw its arrows, each a five-byte call: over the
    // target at (m_AnkX, m_AnkY), and over the candidate while a sub-target is picked at (m_SubAnkX, m_SubAnkY). Both go
    // to one shape draw, a thiscall that takes x, y, scale x, scale y, color and three zeros and pops them.
    struct ArrowCalls
    {
        LocateProblem problem = LocateProblem::None;
        uint32_t target = 0, picked = 0, draw = 0;
    };
    constexpr uint32_t kArrowArgumentBytes = 32;

    ArrowCalls LocateArrowCalls(const ImageSection& text);

    // The routine the target window's draw asks for the color of its arrow over the candidate being picked: a thiscall
    // on the window taking your actor, the candidate's and the colors for in range, out of range and the third, and
    // answering with one of them. It only reads.
    struct RangeColor
    {
        LocateProblem problem = LocateProblem::None;
        uint32_t routine      = 0; // RVA
    };
    RangeColor LocateRangeColor(const ImageSection& text);

    // Where the game guesses its window's client area as GetWindowRect less two SM_CXFRAME across and two SM_CYFRAME
    // and SM_CYCAPTION down (the borders zeroed only in window mode 3): when it reads a mouse message's position, when
    // it moves Windows' pointer to its own, and when the pointer at its area's edge pans the camera. Each is a call to
    // GetWindowRect through the import table and a load of GetSystemMetrics from it into a register, six bytes each.
    struct WindowGuess
    {
        uint32_t windowRectCall = 0, metricsLoad = 0; // RVAs
        uint8_t movImmediate = 0;                     // the opcode of mov reg, imm32 into the register metricsLoad fills
    };
    struct PointerMapping
    {
        LocateProblem problem = LocateProblem::None;
        std::vector<WindowGuess> guesses;
    };
    PointerMapping LocatePointerMapping(const ImageSection& text);

    // The game's mouse controller and its routine that shows or hides the pointer (a thiscall taking 1 to show and
    // popping it), found through the game's own call to it on a mouse move. The controller is a global whose address
    // the code holds; its byte at kPointerShownOffset is 1 while the pointer shows.
    struct PointerShow
    {
        LocateProblem problem   = LocateProblem::None;
        uint32_t controllerGlobal = 0; // the global's address in the running client, as the code holds it
        uint32_t show             = 0; // RVA
    };
    constexpr uint32_t kPointerShownOffset = 0x4E;
    PointerShow LocatePointerShow(const ImageSection& text);

    // The game's command check ends by turning mouse use on (1) for a few mouse commands and off (0) for the rest, which
    // hides the pointer whenever a key sets off a command: two thiscalls into one routine on the mouse controller.
    struct KeyHide
    {
        LocateProblem problem = LocateProblem::None;
        uint32_t offCall = 0, mouseUse = 0; // RVAs: the five-byte call with 0, and the routine
    };
    KeyHide LocateKeyHide(const ImageSection& text);

    // The draw loop's call to the menu window draw: a thiscall on the window's MCD with no stack arguments. RVAs of the
    // five-byte call and of the routine.
    struct WindowDraw
    {
        LocateProblem problem = LocateProblem::None;
        uint32_t call = 0, draw = 0;
    };
    WindowDraw LocateWindowDraw(const ImageSection& text);
    struct WindowMoveSite
    {
        LocateProblem problem = LocateProblem::None;
        uint32_t moveTo = 0; // RVA
    };
    // A window's MCD holds its descriptor at +0x04, whose 16-byte name is at +0x46, and its owner at +0x0C; the owner
    // holds its MCD at +0x08.
    constexpr uint32_t kWindowDescriptor = 0x04, kDescriptorName = 0x46, kMcdOwner = 0x0C, kOwnerMcd = 0x08;
    constexpr size_t kWindowNameLength = 16;
    // The game's windows HeadsUp keeps from being drawn.
    struct HiddenWindows
    {
        bool helpBar = false; // the title and help windows
        bool netstat = false; // the connection readout
        bool Any() const { return helpBar || netstat; }
    };
    // Whether the window with this 16-byte name is one of those hidden.
    bool SkipsWindow(const char* name, HiddenWindows hidden);

    // The game's own move of a window, MCD::MoveTo(int16 x, int16 y) (RVA 0x119110 in the client of 2026-10-07): a
    // thiscall on the MCD that pops its two arguments. It moves the frame and every entry, so drawing and the mouse's
    // hit tests follow, then has the owner lay out its own content again.
    WindowMoveSite LocateWindowMove(const ImageSection& text);
    // An MCD's rectangle in the game's menu resolution: int16 left, top, right and bottom from +0x3A. +0x94 is a move's
    // countdown, kMoveIdle while none is under way; +0x99 is set for windows kept in a 640 x 480 space instead; +0x10 is
    // kWindowOpening while the window has no owner to lay out yet.
    constexpr uint32_t kMcdType = 0x10, kMcdRect = 0x3A, kMcdMove = 0x94, kMcdVirtual = 0x99;
    // Where the window was last moved to (int16 x, y), which its move routine writes before it lays the window out.
    // The owners of the game's timer bars (the cast bar's update at RVA 0x12C410, and its neighbours') compare it
    // with their own spot every frame and move the window back when it differs.
    constexpr uint32_t kMcdAsked = 0x52;
    constexpr int32_t kMoveIdle      = -999;
    constexpr uint32_t kWindowOpening = 0x0E;
    // A menu window's name past its "menu    ", padded with spaces as the game's 16-byte names are: "equip   ".
    constexpr size_t kWindowKeyLength = 8;
    using WindowKey = std::array<char, kWindowKeyLength>;
    // The key of the window with this 16-byte name; none for a name that is not a menu window's.
    std::optional<WindowKey> WindowKeyOf(const char* name);
    // As the settings and the menu write it, without its padding; a key from those, none for one no window could have.
    std::string ShortName(const WindowKey& key);
    std::optional<WindowKey> KeyFromShortName(std::string_view name);
    // Every window HeadsUp may move but the strip's own: the title, help and connection readout windows.
    bool MovableWindow(const WindowKey& key);
    // The equipment window and those that open with it: what is moved until the player picks.
    const std::vector<std::string>& DefaultMovedWindows();
    // From the settings file, short names a space apart: one no window could have, a strip window and a repeat go, and
    // so does any past kMaxWindowKeys.
    std::vector<std::string> MovedWindowsFrom(std::string_view names);
    std::string MovedWindowNames(const std::vector<std::string>& windows);
    // Window keys without allocating, for the game's draw loop: at most kMaxWindowKeys, each once.
    constexpr size_t kMaxWindowKeys = 128;
    class WindowKeyList
    {
    public:
        bool Has(const WindowKey& key) const;
        // False when it is there already or the list is full.
        bool Add(const WindowKey& key);
        void Clear() { m_Count = 0; }
        size_t Size() const { return m_Count; }
        const WindowKey& operator[](size_t i) const { return m_Keys[i]; }

    private:
        std::array<WindowKey, kMaxWindowKeys> m_Keys{};
        size_t m_Count = 0;
    };
    // How far the player moves a window from where the game puts it, in the game's menu resolution.
    struct WindowOffset
    {
        std::string name; // its short name
        int x = 0, y = 0;
        bool operator==(const WindowOffset&) const = default;
    };
    constexpr int kMaxWindowOffset = 2000;
    // From the settings file, "equip:10,-20 statcom:0,40": a name no window could have, a strip window, a repeat, an
    // offset of nothing and any past kMaxWindowKeys go; offsets are clamped to kMaxWindowOffset either way.
    std::vector<WindowOffset> WindowOffsetsFrom(std::string_view text);
    std::string WindowOffsetText(const std::vector<WindowOffset>& offsets);

    // Windows the player moves together: each goes down under the help bar when the group does, and moves by the
    // group's offset before its own, so they keep their places beside each other. A window is in one group at most.
    struct WindowGroup
    {
        std::string name;
        int x = 0, y = 0;
        bool underBar = false;
        std::vector<std::string> windows; // short names
        bool operator==(const WindowGroup&) const = default;
    };
    constexpr size_t kMaxWindowGroups = 24, kMaxGroupName = 24;
    // From the settings file, "Equipment:20,-10,1:equip+inventor|Status:0,40,0:statcom": a group's name keeps
    // letters, digits and spaces, up to kMaxGroupName; a window no group could hold, or one an earlier group has, goes,
    // and so does any group past kMaxWindowGroups or without a name.
    std::vector<WindowGroup> WindowGroupsFrom(std::string_view text);
    // The windows the game opens together, as it opened them with HeadsUp watching (/hu windowlog), each named for its
    // menu and going under the bar as one; a window shared by several menus is in the first.
    std::vector<WindowGroup> DefaultWindowGroups();
    std::string WindowGroupsText(const std::vector<WindowGroup>& groups);

    // What the window hook is given: the windows that go under the bar and each window's whole offset, its own and
    // its group's.
    struct WindowMoves
    {
        std::vector<std::string> underBar;
        std::vector<WindowOffset> offsets;
        bool operator==(const WindowMoves&) const = default;
    };
    WindowMoves CombinedMoves(const std::vector<std::string>& underBar, const std::vector<WindowOffset>& offsets,
        const std::vector<WindowGroup>& groups);

    // Every window HeadsUp can move, by short name: those it has a name for first, then the rest of the client's.
    const std::vector<std::string>& AllWindows();
    // What the player knows a window as, for those HeadsUp has a name for; nullptr for the rest.
    const char* WindowLabel(std::string_view shortName);
    // In the menu resolution: the help bar's lowest reach (0 without it), and the screen's right and bottom.
    struct HelpBand
    {
        int bottom = 0, screenRight = 0, screenBottom = 0;
        int topBar        = 0;     // the top bar's lowest reach, the help bar's not counted; 0 without the bar
        bool allUnderBar  = false; // every window opening under the bar goes below it, not only those picked
        bool moveGameHelp = false; // the game's own help bar goes under the top bar, and the rest under it
    };
    // The game's own help bar's windows: its title and help.
    bool HelpStripWindow(const WindowKey& key);
    // The title alone.
    bool TitleWindow(const WindowKey& key);
    // How far the moved windows go down: from under the game's own strip, whose bottom is stripBottom, to under band,
    // or under the strip itself once it moved down under the top bar.
    int WindowShift(const HelpBand& band, int stripBottom);

    struct WindowSpot
    {
        int left = 0, top = 0;
        bool operator==(const WindowSpot&) const = default;
    };
    // A text the game places once, where its window then was, kept with a window HeadsUp moves: the strip's title,
    // whose text object holds where it is drawn (kStripTextX) and is not told when its window moves.
    class PlacedText
    {
    public:
        // Where the text belongs now. seen: where it is; origin: where the game put its window; at: where the window
        // is. A place other than the one last given back is the game's own, placed for the window at origin.
        WindowSpot Follow(WindowSpot seen, WindowSpot origin, WindowSpot at);
        // Where the game had it, for putting it back: seen itself unless it is where Follow last put it.
        WindowSpot Natural(WindowSpot seen) const;

    private:
        WindowSpot m_Natural{}, m_Given{};
        bool m_Moved = false;
    };

    // The equipment window's owner (ctor RVA 0x131940, draw 0x132F60) places its figures once, as number objects of
    // kNumberStride bytes with an int16 x at +4 and y at +6: Attack, Defense and the 8 resistances at +0x88, and the
    // 16 slots' counts at +0x1CC. Its layout is the base one, so a move leaves them where they were.
    struct NumberRun
    {
        uint32_t at;
        uint32_t count;
    };
    inline constexpr NumberRun kEquipNumbers[] = {{0x88, 10}, {0x1CC, 16}};
    constexpr uint32_t kNumberStride = 0x20, kNumberX = 4, kNumberY = 6;
    // A number at x, y that is still in the window's old place, from at for width by height, moves with it to to;
    // one the game already placed elsewhere, or never placed (at 0, 0), stays.
    WindowSpot MovedNumber(WindowSpot number, WindowSpot at, int width, int height, WindowSpot to);
    // Where each moved window came from and went, so every one moves by the same amount and keeps its place beside
    // the others: a window that is not where HeadsUp put it was put there by the game, and moves from there. At most
    // kMaxWindowKeys windows, the oldest forgotten first; nothing is allocated.
    class WindowPlacements
    {
    public:
        // Where to move the window at mcd, now at now and width by height, or none when it is where it should be:
        // down by shift and over by dx, dy from where the game put it, kept on the screen. A window moved down only
        // never goes up, however tall it is.
        // The shift applies only to a window the game put above shiftAbove, and with follow to one that such a window,
        // gone down, would lie over (the gil box under the status window), and so on down from it.
        std::optional<WindowSpot> PlaceFor(uint32_t mcd, const WindowKey& key, WindowSpot now, int width, int height,
            int dx, int dy, int shift, int shiftAbove, int screenRight, int screenBottom, bool follow = false);
        // Where the game put the window at mcd, now at now: where HeadsUp moved it from, or now when it is not.
        WindowSpot OriginOf(uint32_t mcd, const WindowKey& key, WindowSpot now) const;
        // The frame PlaceFor stamps the windows it sees with from now on. The frame that ends says which windows
        // follow another down in the next.
        void SetFrame(uint32_t frame);
        // Whether this window's owner places it itself: it was back where the game had it right after HeadsUp moved
        // it, kBounces times with at most kBounceFrames between, as no page change or relayout does. HeadsUp's move
        // of such a window must leave kMcdAsked as the owner set it, or the two move it back and forth for good.
        bool PutsItselfBack(const WindowKey& key) const { return m_SelfPlaced.Has(key); }
        static constexpr int kBounces           = 3;
        static constexpr uint32_t kBounceFrames = 2;

        struct Placed
        {
            uint32_t mcd = 0;
            WindowKey key{};
            WindowSpot from{}, to{};
            int width = 0, height = 0;
            int shift    = 0;     // what PlaceFor last offered it
            bool root   = false; // it went down in its own right: above shiftAbove
            bool pushed = false; // one that went down in the frame before would lie over where the game put it
            uint32_t frame = 0; // the last frame PlaceFor saw it
            uint32_t undone = 0; // the last frame it was found back where the game had it
            int bounces     = 0; // how many times running
        };
        // The windows HeadsUp moved that drew at frame since or later: to put back from to to from.
        std::vector<Placed> MovedSince(uint32_t since) const;

    private:
        std::array<Placed, kMaxWindowKeys> m_Placed{};
        size_t m_Next    = 0; // the slot a new window takes
        uint32_t m_Frame = 0;
        WindowKeyList m_SelfPlaced;
    };

    // The game's top strip keeps its two strings in one text class (constructor RVA 0x220FD0): four levels of buffer,
    // the highest non-empty one shown. kStripShown points into the shown buffer, 0 while nothing is shown; kStripBusy is
    // non-zero while the help window shows its busy shape instead of text; kStripLevel is the shown level.
    constexpr uint32_t kStripShown = 0x48, kStripBusy = 0x64, kStripLevel = 0x70;
    // Where the text is drawn, in the menu's resolution: two int32s, x then y. The help line's follows its window; the
    // title's is set without regard to where its window is (seen 2026-10-12: at 20, 24 with the window moved down).
    constexpr uint32_t kStripTextSpot = 0x58;
    constexpr uint32_t kStripBuffers = 0x74, kStripStride = 0xCB, kStripBufferBytes = 0xCA, kStripLevels = 4;
    constexpr uint32_t kStripObjectBytes = kStripBuffers + kStripLevels * kStripStride; // where the scroll state starts

    enum class StripWindow : uint8_t
    {
        Help,  // "menu    helpwind": the help line
        Title, // "menu    titlewin": the caption
    };
    struct StripSites
    {
        LocateProblem problem = LocateProblem::None;
        uint32_t helpSlot = 0, titleSlot = 0; // where each owner's address is kept, as the running client holds them
        uint32_t helpText = 0, titleText = 0; // the text object's offset in its owner
    };
    // From the menu catalog's records in data and the setters in text, each checked against the other.
    StripSites LocateStripText(const ImageSection& text, const ImageSection& data);
    // Whether a window's 16-byte name is that strip window's.
    bool IsStripWindow(const char* name, StripWindow window);
    // The client's connection readout reads three int32s at kNetCounters in its network object, a global pointer
    // that is 0 until a session starts: bytes received a second and bytes sent a second, each averaged over the last
    // 3 seconds, and a meter from 0 to 100 that stays full while anything arrives within 10 seconds.
    constexpr uint32_t kNetCounters = 0x4638;
    struct NetStats
    {
        LocateProblem problem = LocateProblem::None;
        uint32_t global       = 0; // the pointer's address in the running client, as the code holds it
    };
    NetStats LocateNetStats(const ImageSection& text);

    // The client keeps the zone's weather as a uint16 Weather id in a global, which its weather effects read with
    // mov ax,[global]; cmp ax,4; jb; cmp ax,20.
    struct WeatherSite
    {
        LocateProblem problem = LocateProblem::None;
        uint32_t global       = 0; // the weather's address in the running client, as the code holds it
    };
    WeatherSite LocateWeather(const ImageSection& text);

    struct NetCounters
    {
        int32_t received = 0, sent = 0, health = 0;
        bool operator==(const NetCounters&) const = default;
    };
    constexpr int32_t kFullHealth = 100;
    constexpr int32_t kMaxRate    = 1000000; // bytes a second, far past what the game sends
    // The three as read; nothing when they cannot be the client's, as when the object is not what HeadsUp expects.
    std::optional<NetCounters> CountersFrom(int32_t received, int32_t sent, int32_t health);
}
