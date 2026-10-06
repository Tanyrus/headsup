#pragma once

#include "Ashita.h"
#include "outline.h"
#include "tracker.h"

#include <cstdint>
#include <string>
#include <vector>

namespace aggroglow
{
    // Draws each outlined mob's label centered above its nameplate with a pool of Ashita font objects.
    class LabelRenderer
    {
    public:
        static constexpr size_t kMaxLabels = 64;

        struct Shown
        {
            uint16_t index; // entity target index
            float x, y, width, height;
        };

        void Initialize(IFontManager* fonts) { m_Fonts = fonts; }

        // Shows a label for each outlined mob the camera can see (LabelVisible) above its nameplate box from the latest
        // OutlineRenderer::FinishText, and hides the rest of the pool.
        void Update(const Tracker& tracker, const OutlineRenderer& outline, bool show);

        // Deletes the font objects.
        void Release();

        const std::vector<Shown>& LastShown() const { return m_Shown; }
        uint32_t TextChangesLastUpdate() const { return m_TextChanges; } // for /ag debug

        // True once, when a font object could not be created; labels then stay off until the plugin reloads.
        bool TakeFailure();

    private:
        IFontObject* Object(size_t i);

        IFontManager* m_Fonts = nullptr;
        std::vector<IFontObject*> m_Objects;
        std::vector<std::string> m_Texts; // what each object shows, to skip unchanged SetText calls
        std::vector<uint32_t> m_Colors;
        std::vector<Shown> m_Shown;
        uint32_t m_TextChanges = 0;
        bool m_Failed         = false;
        bool m_FailurePending = false;
    };
}
