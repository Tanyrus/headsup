#pragma once

#include "Ashita.h"
#include "icons.h"
#include "outline.h"
#include "settings.h"
#include "tracker.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace aggroglow
{
    // Draws each visible mob's nameplate with Ashita font and primitive objects: the icon row, the level and con line
    // and, in replace mode, the name (spec sections 1 and 5). A mob keeps its own two font objects while its nameplate
    // is shown; icons come from one pool per icon kind, so each texture is loaded once.
    class NameplateRenderer
    {
    public:
        static constexpr size_t kMaxPlates = 64;

        // Where a mob's nameplate went, for /ag debug.
        struct Shown
        {
            uint16_t index;
            float nameX, nameY, labelX, labelY, iconsX, iconsY;
            int nameHeight, labelHeight, iconSize, icons;
            uint32_t nameColor;
        };

        void Initialize(IFontManager* fonts, IPrimitiveManager* primitives)
        {
            m_Fonts      = fonts;
            m_Primitives = primitives;
        }

        // Shows the nameplate of every mob the camera can see (LabelVisible) and hides everything else.
        void Update(const Tracker& tracker, const OutlineRenderer& outline, const Settings& settings);

        // Deletes every font and primitive object.
        void Release();

        const std::vector<Shown>& LastShown() const { return m_Shown; }
        // Mobs whose names this frame replaces. Their name is drawn only from the second such frame, once the game's
        // letters are hidden, so the two never overlap.
        const std::vector<uint16_t>& ReplacingNames() const { return m_Replacing; }

        // True once, when a font object could not be created; names and labels stay off until the plugin reloads.
        bool TakeFailure();
        // True once, when an icon texture could not be loaded; icons stay off until the plugin reloads.
        bool TakeIconFailure();

    private:
        struct Text
        {
            IFontObject* font = nullptr;
            std::string text;
            uint32_t color   = 0;
            uint32_t outline = 0;
            int height       = 0;
            int family     = -1;
            bool bold      = false;
            bool visible   = false;
        };

        struct Plate
        {
            Text name;
            Text label;
            uint32_t frame = 0;
        };

        Plate* PlateFor(uint16_t index);
        void Show(Text& t, const char* text, uint32_t color, int height, const Settings& settings, SIZE& size);
        void Place(Text& t, float x, float y);
        void Hide(Text& t);
        IPrimitiveObject* IconObject(Icon icon, size_t n);

        IFontManager* m_Fonts           = nullptr;
        IPrimitiveManager* m_Primitives = nullptr;
        std::vector<Plate> m_Plates;                  // slots; index i owns aliases aggroglow_name_<i> and _label_<i>
        std::unordered_map<uint16_t, size_t> m_SlotOf; // entity index -> slot
        std::vector<size_t> m_FreeSlots;
        std::vector<IPrimitiveObject*> m_Icons[kIconCount];
        size_t m_IconsUsed[kIconCount] = {};
        std::vector<Shown> m_Shown;
        std::vector<uint16_t> m_Replacing;
        std::unordered_set<uint16_t> m_ReplacedLast;
        uint32_t m_Frame       = 0;
        bool m_Failed          = false;
        bool m_FailurePending  = false;
        bool m_IconsFailed     = false;
        bool m_IconFailurePending = false;
    };
}
