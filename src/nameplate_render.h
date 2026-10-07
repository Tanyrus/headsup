#pragma once

#include "Ashita.h"
#include "game_cursor.h"
#include "game_names.h"
#include "icons.h"
#include "nameplate.h"
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
        void Update(const Tracker& tracker, const GameNames& names, const Settings& settings, float toX, float toY,
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
        // A cursor's text is empty and its font names its shape.
        struct TextureKey
        {
            std::string text, font;
            bool bold = false;
            int height = 0;
            uint32_t color = 0, outline = 0;
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
        };

        struct Plate
        {
            PlateTexture name;
            PlateTexture label;
            PlateTexture cursor;
            int nameRaster   = 0;
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

        bool Prepare(PlateTexture& t, const char* text, uint32_t color, int pixelHeight, const Settings& settings);
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
