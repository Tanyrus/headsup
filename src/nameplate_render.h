#pragma once

#include "Ashita.h"
#include "frame_cache.h"
#include "game_cursor.h"
#include "game_names.h"
#include "gpu.h"
#include "icons.h"
#include "look.h"
#include "nameplate.h"
#include "native_hook.h"
#include "ph_timers.h"
#include "settings.h"
#include "plate_image.h"
#include "tracker.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace headsup
{
    class NameplateRenderer
    {
    public:
        // For /hu debug and the draw dump.
        struct Shown
        {
            uint16_t index;
            float nameX, nameY, labelX, labelY, iconsX, iconsY;
            int nameHeight, labelHeight, iconSize, mobIconCount;
            uint32_t nameColor;
        };

        void SetDevice(IDirect3DDevice8* device) { m_Device = device; }
        // toX and toY turn back-buffer pixels into those of the image the nameplates are drawn into.
        void Update(const Tracker& tracker, const GameNames& names, const NameHook& hook, const Settings& settings, float toX, float toY,
            const CursorTargets& targets, double now, uint16_t selfIndex, const std::vector<TimerLine>& selfTimers);
        void Clear();
        void ReleasePlates();
        // Changes render states.
        void Draw(bool depthTest);
        void Release();

        const std::vector<Shown>& LastShown() const { return m_Shown; }
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
            size_t markAt      = Label::kNoMark;
            uint32_t markColor = 0;
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
        };

        struct Plate
        {
            PlateTexture name;
            PlateTexture label;
            PlateTexture cursor;
            PlateTexture ornament;
            float scale      = 0.0f; // eased toward the game's name size
            double scaleTime = 0.0;
            float rise       = 0.0f; // screen pixels above its own place, eased toward clear of the plates under it
            double riseTime  = 0.0;
            int nameRaster   = 0;
            int ornamentRaster = 0;
            int labelRaster  = 0;
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
        // The ornament's diamond has the shadow of shadowLine.
        bool PrepareOrnament(PlateTexture& t, uint32_t color, int pixelHeight, int width, const LineStyle& shadowLine);
        bool PrepareCursor(PlateTexture& t, uint32_t color, int pixelHeight, const Settings& settings);
        bool Upload(PlateTexture& t, const Image& image, TextureKey key);
        void FailNames(std::string what);
        static void ReleasePlate(Plate& plate);

        IDirect3DDevice8* m_Device = nullptr;
        GdiText m_Gdi;
        std::unordered_map<uint16_t, Plate> m_Plates;
        FrameCache<BadgeKey, PlateTexture> m_Badges;
        IconTextures m_Icons;
        std::vector<Quad> m_Quads;
        std::vector<Shown> m_Shown;
        std::vector<CursorName> m_CursorNames;
        uint32_t m_Frame           = 0;
        double m_RedrawMs          = 0.0; // spent this frame
        bool m_NamesFailed         = false;
        std::string m_NameFailure;
    };
}
