#pragma once

#include "Ashita.h"
#include "game_cursor.h"
#include "game_names.h"
#include "icons.h"
#include "nameplate.h"
#include "settings.h"
#include "text_image.h"
#include "tracker.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace headsup
{
    // Draws each entity's nameplate as textured quads: the name of any entity whose kind is replaced, with a player's
    // icons beside it; a mob's level line and MobDB icons; and the cursor over the target or the sub-target candidate.
    // Text and the cursor are drawn with GDI into a texture per entity and line, redrawn only when what they show
    // changes; each icon has one texture.
    class NameplateRenderer
    {
    public:
        // Where an entity's nameplate went, for /hu debug.
        struct Shown
        {
            uint16_t index;
            float nameX, nameY, labelX, labelY, iconsX, iconsY;
            int nameHeight, labelHeight, iconSize, icons;
            uint32_t nameColor;
        };

        void SetDevice(IDirect3DDevice8* device) { m_Device = device; }
        // Lays out the nameplate of every entity whose name is on screen in the pixels of the image it will be drawn
        // into: toX and toY turn back-buffer pixels into those (1 for the back buffer itself). now, in seconds, makes the
        // cursor bob.
        void Update(const Tracker& tracker, const GameNames& names, const Settings& settings, float toX, float toY,
            const CursorTargets& targets, double now);
        // Forgets the last layout, for a frame whose nameplates were not drawn.
        void Clear();
        // Releases the plates' textures too, while nameplates are off.
        void ReleasePlates();
        // Draws the last layout into the bound render target, farthest first and cursors last. With depthTest, each
        // nameplate sits at its name's depth, so whatever the scene has in front covers it. Changes render states.
        void Draw(bool depthTest);
        // Releases every texture.
        void Release();

        const std::vector<Shown>& LastShown() const { return m_Shown; }
        // The entities that got HeadsUp's cursor in the last layout: the game's cursor there is blocked.
        const std::vector<CursorName>& CursorNames() const { return m_CursorNames; }
        // Once a texture for names or for icons could not be made, that part stays off until the plugin reloads.
        bool NamesFailed() const { return m_Failed; }
        bool IconsFailed() const { return m_IconsFailed; }
        // What failed, once, after it did; empty otherwise.
        std::string TakeFailure();
        std::string TakeIconFailure();

    private:
        // What a texture shows: its text (empty for a cursor), font (or cursor shape), weight, size and colors.
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
            float width  = 0.0f;      // pixels of the drawn image
            float height = 0.0f;
            float u = 0.0f, v = 0.0f; // the image's share of its power-of-two texture
            float tip = 0.5f;         // a cursor's point, across the image's width
        };

        struct Plate
        {
            PlateTexture name;
            PlateTexture label;
            PlateTexture cursor;
            int nameRaster   = 0; // the pixel height each is drawn at, for RasterHeight
            int labelRaster  = 0;
            int cursorRaster = 0;
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
        const IconTexture* Icon(Icon icon);
        void Fail(std::string what);

        IDirect3DDevice8* m_Device = nullptr;
        std::unordered_map<uint16_t, Plate> m_Plates; // by entity index
        IconTexture m_IconTextures[kIconCount] = {};
        std::vector<Quad> m_Quads;
        std::vector<Shown> m_Shown;
        std::vector<CursorName> m_CursorNames;
        uint32_t m_Frame           = 0;
        bool m_Failed              = false;
        bool m_IconsFailed         = false;
        std::string m_Failure;     // what failed, until taken
        std::string m_IconFailure;
    };
}
