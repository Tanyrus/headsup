#pragma once

#include "Ashita.h"
#include "icons.h"
#include "nameplate.h"
#include "text_image.h"
#include "outline.h"
#include "settings.h"
#include "tracker.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace headsup
{
    // Draws each visible entity's nameplate as textured quads: for a mob the icon row and the level and con line, for any
    // entity whose kind is replaced its name, and for the target the cursor. Text is drawn with GDI into a texture per
    // entity and line, redrawn only when the text, font, size or colors change; each icon has one texture.
    class NameplateRenderer
    {
    public:
        static constexpr size_t kMaxPlates = 128;

        // Where an entity's nameplate went, for /hu debug.
        struct Shown
        {
            uint16_t index;
            float nameX, nameY, labelX, labelY, iconsX, iconsY;
            int nameHeight, labelHeight, iconSize, icons;
            uint32_t nameColor;
        };

        // The entities that get a target cursor, by entity index (0 for none): the target and, while a sub-target is
        // being picked, the candidate under the sub-target cursor. locked: the player is locked on to the target.
        struct CursorTargets
        {
            uint16_t target    = 0;
            uint16_t subTarget = 0;
            bool locked        = false;
        };

        void SetDevice(IDirect3DDevice8* device) { m_Device = device; }
        // Lays out the nameplate of every mob the camera can see (LabelVisible) in the pixels of the image it will be
        // drawn into: toX and toY turn back-buffer pixels into those (1 for the back buffer itself).
        // now, in seconds, makes the target cursor bob.
        void Update(const Tracker& tracker, const OutlineRenderer& outline, const Settings& settings, float toX, float toY,
            const CursorTargets& cursors, double now);
        // Forgets the last layout, for a frame whose nameplates could not be drawn.
        void Clear();
        // Draws the last layout into the bound render target. With depthTest, each nameplate sits at its name's depth,
        // so whatever the scene has in front of the name covers it. Changes render states: the caller restores them.
        void Draw(bool depthTest);
        // Releases every texture.
        void Release();

        const std::vector<Shown>& LastShown() const { return m_Shown; }
        // Mobs whose names this frame replaces. Their name is drawn only from the second such frame, once the game's
        // letters are hidden, so the two never overlap.
        const std::vector<uint16_t>& ReplacingNames() const { return m_Replacing; }
        // The names that got our target cursor in the last layout: the game's cursor over them is hidden.
        const std::vector<CursorName>& CursorNames() const { return m_CursorNames; }
        // True once, when a text texture could not be made; names and labels stay off until the plugin reloads.
        bool TakeFailure();
        // True once, when an icon texture could not be made; icons stay off until the plugin reloads.
        bool TakeIconFailure();

    private:
        struct TextTexture
        {
            IDirect3DTexture8* texture = nullptr;
            std::string key;          // the text, font, size and colors it was drawn with
            float width  = 0.0f;      // pixels of the drawn image
            float height = 0.0f;
            float u = 0.0f, v = 0.0f; // the image's share of its power-of-two texture
            float tip = 0.5f;         // a cursor's point, across the image's width
        };

        struct Plate
        {
            TextTexture name;
            TextTexture label;
            TextTexture cursor;
            int nameRaster   = 0; // the pixel height each is drawn at, for RasterHeight
            int labelRaster  = 0;
            int cursorRaster = 0;
            uint32_t frame  = 0;
        };

        struct Quad
        {
            IDirect3DTexture8* texture;
            float x, y, width, height, u, v, depth;
            uint32_t tint;
        };

        bool Prepare(TextTexture& t, const char* text, uint32_t color, int pixelHeight, const Settings& settings);
        bool PrepareCursor(TextTexture& t, uint32_t color, int pixelHeight, const Settings& settings);
        bool Upload(TextTexture& t, const Image& image, std::string key);
        IDirect3DTexture8* CreateTexture(const void* bgra, int width, int height, float& u, float& v);
        IDirect3DTexture8* IconTexture(Icon icon);

        IDirect3DDevice8* m_Device = nullptr;
        std::unordered_map<uint16_t, Plate> m_Plates; // by entity index
        IDirect3DTexture8* m_IconTextures[kIconCount] = {};
        std::vector<Quad> m_Quads;
        std::vector<Shown> m_Shown;
        std::vector<uint16_t> m_Replacing;
        std::vector<CursorName> m_CursorNames;
        std::unordered_set<uint16_t> m_ReplacedLast;
        uint32_t m_Frame          = 0;
        bool m_Failed             = false;
        bool m_FailurePending     = false;
        bool m_IconsFailed        = false;
        bool m_IconFailurePending = false;
    };
}
