#pragma once

#include "Ashita.h"
#include "frame_cache.h"
#include "game_cursor.h"
#include "game_names.h"
#include "gpu.h"
#include "look.h"
#include "nameplate.h"
#include "native_hook.h"
#include "ph_timers.h"
#include "settings.h"
#include "plate_image.h"
#include "plate_layout.h"
#include "tracker.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace headsup
{
    class NameplateRenderer
    {
    public:
        // For /hu debug and the draw dump: a line's text runs joined, where its box starts, the size its text is shown
        // at (its icons' on a line without text) and how many icons it drew.
        struct ShownLine
        {
            std::string text;
            float x, y;
            int height;
            int icons;
        };
        // The name fields are the plate's first text run holding the entity's name: 0 and white without one.
        struct Shown
        {
            uint16_t index;
            float nameX, nameY;
            int nameHeight;
            uint32_t nameColor;
            std::vector<ShownLine> lines; // the plate's lines, a design line's rows each
            int lineCount;
        };

        void SetDevice(IDirect3DDevice8* device);
        // toX and toY turn back-buffer pixels into those of the image the nameplates are drawn into.
        void Update(const Tracker& tracker, const GameNames& names, const NameHook& hook, const Settings& settings, float toX, float toY,
            const CursorTargets& targets, double now, uint16_t selfIndex, const std::vector<TimerLine>& selfTimers);
        void Clear();
        void ReleasePlates();
        // Changes render states.
        void Draw(bool depthTest);
        // The cursors of targets whose nameplate carries none, at the target window's anchors, as far as those are on
        // screen. Read the anchors when these are drawn: the game moves them only after it has copied its scene.
        std::vector<LoneCursor> LoneCursorsDue(const Settings& settings, const CursorTargets& targets, float screenWidth,
            float screenHeight) const;
        // In the back buffer's own pixels, at exactly the cursor's size and on top: there is no name to scale by or to
        // sit behind walls with. Changes render states.
        void DrawLoneCursors(const std::vector<LoneCursor>& lone, const Settings& settings, double now, float screenWidth,
            float screenHeight);
        void Release();

        const std::vector<Shown>& LastShown() const { return m_Shown; }
        // How much of the screen's top the top panel covers, in back-buffer pixels: cursors stay below it.
        void SetCoveredTop(float pixels) { m_CoveredTop = pixels; }
        // The picture a cursor of the File style draws, or none for a file that could not be read.
        void SetCursorFile(std::optional<Image> picture);
        // The game's own font, for the text set in it; every plate is drawn again with it.
        void SetGameFont(std::optional<GameFontFace> face);
        // The game's cursor is blocked over these.
        const std::vector<CursorName>& CursorNames() const { return m_CursorNames; }
        bool NamesFailed() const { return m_NamesFailed; }
        bool IconsFailed() const { return m_Icons.Failed(); }
        std::string TakeNameFailure();
        std::string TakeIconFailure();

    private:
        // How a texture's text is drawn. A cursor's line font names its shape and its shadow color is its outline's.
        struct TextLook
        {
            LineStyle line;
            uint32_t color     = 0;
            uint32_t glow      = 0; // 0 for none
            float glowStrength = 0.0f, glowSize = 0.0f;
            bool operator==(const TextLook&) const = default;
        };

        // The ornament's and the cursor's text is empty; only the ornament has a width.
        struct TextureKey
        {
            std::string text;
            int height = 0, width = 0;
            TextLook look;
            bool operator==(const TextureKey&) const = default;
        };

        struct PlateTexture
        {
            IDirect3DTexture8* texture = nullptr;
            TextureKey key;
            float width  = 0.0f;
            float height = 0.0f;
            float u = 0.0f, v = 0.0f; // the image's share of its power-of-two texture
            float tip = 0.5f;
            float halo = 0.0f; // the shadow's or glow's margin, in texture pixels: it hangs outside the box the layout places
            int raster = 0;     // the height it is drawn at: its key's, or less when that image fits no texture
            bool unfit = false; // it fits none at the smallest height either: no texture, and the key says for what
        };

        // One of a plate's lines: a texture for each of its text runs, in order, and the ornament above it.
        struct LineTextures
        {
            std::vector<PlateTexture> texts;
            PlateTexture ornament;
            int raster         = 0;
            int ornamentRaster = 0;
        };

        struct Plate
        {
            LineTextures lines[kPlateLines]; // by the composed line's source
            PlateTexture cursor;
            PlateTexture cursorUnder; // the target's own, while it is also the candidate being picked
            float scale      = 0.0f; // eased toward the game's name size
            double scaleTime = 0.0;
            float rise       = 0.0f; // screen pixels above its own place, eased toward clear of the plates under it
            double riseTime  = 0.0;
            int cursorRaster = 0;
            std::vector<PlateTexture> timers;
            int timerRaster  = 0;
            int badgeRaster  = 0; // the badge group's text height; the badges themselves are shared (m_Badges)
            uint32_t frame   = 0;
        };

        // A badge's texture is shared by every plate showing the same text in the same color and size.
        struct BadgeKey
        {
            std::string text;
            uint32_t color = 0;
            int height     = 0;
            auto operator<=>(const BadgeKey&) const = default;
        };

        struct Quad
        {
            GpuQuad gpu;
            int rank;
        };

        bool Prepare(PlateTexture& t, const char* text, const TextLook& look, int pixelHeight);
        // The icon's badge from the shared ones: null without one, and for every badge once one could not be made.
        const PlateTexture* PrepareBadge(const IconDraw& icon, const LineStyle& line, int pixelHeight);
        // The ornament's diamond has the shadow of shadowLine.
        bool PrepareOrnament(PlateTexture& t, uint32_t color, int pixelHeight, int width, const LineStyle& shadowLine);
        bool PrepareCursor(PlateTexture& t, const EntityState& over, int pixelHeight, const Settings& settings);
        Image m_Chocobo;                   // made when first drawn
        std::optional<Image> m_CursorFile; // the File style's picture; none while it is not read
        int m_CursorFileSerial = 0;
        float m_CoveredTop     = 0.0f;
        bool Upload(PlateTexture& t, const Image& image, TextureKey key);
        // Marks t as holding this key with nothing to draw: its image fits no texture at the smallest height either.
        static void LeaveOut(PlateTexture& t, TextureKey key);
        void FailNames(std::string what);
        static void ReleaseLine(LineTextures& line);
        static void ReleasePlate(Plate& plate);

        IDirect3DDevice8* m_Device = nullptr;
        int m_LargestSide          = kFallbackTextureSide; // of a text's texture, from the device
        GdiText m_Gdi;
        std::unordered_map<uint16_t, Plate> m_Plates;
        FrameCache<BadgeKey, PlateTexture> m_Badges;
        IconTextures m_Icons;
        std::vector<Quad> m_Quads;
        std::vector<Shown> m_Shown;
        std::vector<CursorName> m_CursorNames;
        size_t m_PlateCursors = 0; // how many of them, the first ones, sit on a nameplate
        PlateTexture m_LoneCursor;
        PlateTexture m_LoneCursorUnder; // the target's own, while it is also the candidate being picked
        // Kept from plate to plate so measuring one allocates nothing: its lines as the layout takes them, the text
        // boxes of the line being measured, a badge for each of its icons (null without one), which texts are left out
        // and where each line's texts start among its design line's.
        std::vector<LineInput> m_Inputs;
        std::vector<size_t> m_TextStart;
        std::vector<ItemSize> m_TextSizes;
        std::vector<const PlateTexture*> m_IconBadges;
        std::vector<uint8_t> m_LeftOut;
        uint32_t m_Frame           = 0;
        double m_RedrawMs          = 0.0; // spent this frame
        bool m_NamesFailed         = false;
        std::string m_NameFailure;
    };
}
