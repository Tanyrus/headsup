#include "bar_document.h"
#include "window_skin.h"
#include "rml_session.h"
#include "test.h"

#include <RmlUi/Core.h>

#include <optional>
#include <string>
#include <vector>

using namespace headsup;

namespace
{
    constexpr int kBarHeight = 45;

    BarLook LookOf(const BarPalette& p, const Settings& settings)
    {
        const BarMetrics m  = MetricsFor(kBarHeight, p.shape);
        return BarLook{p, settings, m, PillRow(m), TextRow{13, 11, true, 10.0f, 24.0f, 12.0f},
            BarTitleFont(settings, p.shape), p.shape == BarShape::Ornate, BarTextFont(settings), std::nullopt};
    }
}

TEST(rmlui_reads_every_themes_bar_and_popups_without_a_complaint)
{
    const BarTextPlace place{BarLinePlace{16, 9.0f, 14.0f, 2.0f}, BarLinePlace{17, 8.0f, 13.0f, 1.0f},
        BarLinePlace{14, 23.0f, 0.0f, 3.0f},
            BarLinePlace{14, 15.0f, 15.0f, 3.0f}};
    const PopupFaces faces = {PopupFace{"Barlow Condensed", 24, 29, 0}, PopupFace{"Barlow Condensed", 18, 22, 0},
        PopupFace{"Barlow Condensed", 15, 18, 2}, PopupFace{"Barlow Condensed", 15, 18, 0}};
    Settings settings;
    std::vector<BarPalette> palettes;
    for (int t = 0; t < kFixedThemeCount; ++t)
        palettes.push_back(PaletteOf(static_cast<BarTheme>(t)));
    for (const BarShape shape : {BarShape::Ornate, BarShape::Pills, BarShape::Glass})
    {
        BarPalette custom = settings.customPalette;
        custom.shape      = shape;
        custom.border     = 3;
        palettes.push_back(custom);
    }
    struct Drawn
    {
        BarPalette palette;
        std::optional<SkinFrame> skin;
    };
    std::vector<Drawn> drawn;
    for (const BarPalette& palette : palettes)
        drawn.push_back({palette, std::nullopt});
    // The game's art, its corners squeezed on a bar shorter than two of them.
    BarPalette window = settings.customPalette;
    window.shape      = BarShape::Window;
    drawn.push_back({window, std::nullopt});
    drawn.push_back({window, SkinFrame{0, 4.0f / 3.0f, 0.5f, 1.0f}});
    drawn.push_back({window, SkinFrame{7, 3.0f, 0.75f, 0.5f}});
    for (const Drawn& d : drawn)
    {
        BarLook look = LookOf(d.palette, settings);
        look.skin    = d.skin;
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

TEST(the_target_widget_goes_back_to_breathing_after_its_popup_without_a_complaint)
{
    const BarTextPlace place{BarLinePlace{16, 9.0f, 14.0f, 2.0f}, BarLinePlace{17, 8.0f, 13.0f, 1.0f},
        BarLinePlace{14, 23.0f, 0.0f, 3.0f},
            BarLinePlace{14, 15.0f, 15.0f, 3.0f}};
    Settings settings;
    for (int t = 0; t < kFixedThemeCount; ++t)
    {
        test::RmlSession rml;
        Rml::ElementDocument* bar =
            rml.context->LoadDocumentFromMemory(BarDocument(LookOf(PaletteOf(static_cast<BarTheme>(t)), settings), place), "bar");
        bar->Show();
        Rml::Element* target = bar->GetElementById(WidgetId(BarWidget::Target));
        CHECK(target != nullptr);
        // The pointer comes onto the widget (its popup opens), then leaves it (the popup closes).
        for (const bool lit : {true, false})
        {
            target->SetClass(kLitClass, lit);
            for (int frame = 0; frame < 10; ++frame)
            {
                rml.complaints.seconds += 0.05;
                rml.context->Update();
                rml.context->Render();
            }
        }
        CHECK_EQ(rml.Said(), "");
    }
}
