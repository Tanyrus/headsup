#include "bar_document.h"
#include "rml_session.h"
#include "test.h"

#include <RmlUi/Core.h>

#include <string>

using namespace headsup;

namespace
{
    constexpr int kBarHeight = 45;

    BarLook LookOf(BarTheme theme, const Settings& settings)
    {
        const BarPalette& p = PaletteOf(theme);
        const BarMetrics m  = MetricsFor(kBarHeight, p.shape);
        return BarLook{p, settings, m, PillRow(m), TextRow{13, 11, true, 10.0f, 24.0f, 12.0f},
            BarTitleFont(settings, p.shape), p.shape == BarShape::Ornate, BarTextFont(settings)};
    }
}

TEST(rmlui_reads_every_themes_bar_and_popups_without_a_complaint)
{
    const BarTextPlace place{BarLinePlace{16, 9.0f, 14.0f, 2.0f}, BarLinePlace{17, 8.0f, 13.0f, 1.0f},
        BarLinePlace{14, 23.0f, 0.0f, 3.0f}};
    const PopupFaces faces = {PopupFace{"Barlow Condensed", 24, 29, 0}, PopupFace{"Barlow Condensed", 18, 22, 0},
        PopupFace{"Barlow Condensed", 15, 18, 2}, PopupFace{"Barlow Condensed", 15, 18, 0}};
    Settings settings;
    for (int t = 0; t < kBarThemeCount; ++t)
    {
        const BarTheme theme = static_cast<BarTheme>(t);
        const BarLook look   = LookOf(theme, settings);
        test::RmlSession rml;
        Rml::ElementDocument* bar = rml.context->LoadDocumentFromMemory(BarDocument(look, place), "bar");
        Rml::ElementDocument* popups = rml.context->LoadDocumentFromMemory(PopupDocument(look, faces), "popups");
        CHECK(bar != nullptr && popups != nullptr);
        bar->Show();
        popups->Show();
        // A popup that opens: what a theme does as one opens is read only then.
        Rml::Element* popup = popups->GetElementById(kPopupId);
        CHECK(popup != nullptr);
        popup->SetClass(kOpenClass, true);
        rml.context->Update();
        rml.context->Render();
        CHECK_EQ(rml.Said(), "");
    }
}
