#pragma once

#include "Ashita.h"
#include "game_cursor.h"
#include "game_names.h"
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
        bool IconsFailed() const { return m_IconsFailed; }
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
            int nameRaster   = 0;
            int ornamentRaster = 0;
            int labelRaster  = 0;
            int cursorRaster = 0;
            std::vector<PlateTexture> timers;
            int timerRaster  = 0;
            uint32_t frame   = 0;
        };

        struct IconTexture
        {
            IDirect3DTexture8* texture = nullptr;
            float u = 0.0f, v = 0.0f;
        };

        struct Quad
        {
            IDirect3DTexture8* texture;
            float x, y, width, height, u, v, depth;
            uint32_t tint;
        };

        bool Prepare(PlateTexture& t, const char* text, const TextLook& look, int pixelHeight);
        // The ornament's diamond has the shadow of shadowLine.
        bool PrepareOrnament(PlateTexture& t, uint32_t color, int pixelHeight, int width, const LineStyle& shadowLine);
        bool PrepareCursor(PlateTexture& t, uint32_t color, int pixelHeight, const Settings& settings);
        bool Upload(PlateTexture& t, const Image& image, TextureKey key);
        IDirect3DTexture8* CreateTexture(const void* bgra, int width, int height, float& u, float& v);
        const IconTexture* IconTextureFor(Icon icon);
        void FailNames(std::string what);
        static void ReleasePlate(Plate& plate);

        IDirect3DDevice8* m_Device = nullptr;
        std::unordered_map<uint16_t, Plate> m_Plates;
        IconTexture m_IconTextures[kIconCount] = {};
        std::vector<Quad> m_Quads;
        std::vector<Shown> m_Shown;
        std::vector<CursorName> m_CursorNames;
        uint32_t m_Frame           = 0;
        bool m_NamesFailed         = false;
        bool m_IconsFailed         = false;
        std::string m_NameFailure;
        std::string m_IconFailure;
    };
}
