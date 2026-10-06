#include "menu.h"

#include "Ashita.h"

#include <cstdio>

namespace aggroglow
{
    namespace
    {
        const char* const kCategoryLabels[kCategoryCount] = {"Will attack", "Won't attack", "Unknown (no mob data)",
            "NM will attack", "NM won't attack"};
        const char* const kShowIds[kCategoryCount]  = {"##showWillAttack", "##showWontAttack", "##showUnknown",
            "##showNmWillAttack", "##showNmWontAttack"};
        const char* const kColorIds[kCategoryCount] = {"##colorWillAttack", "##colorWontAttack", "##colorUnknown",
            "##colorNmWillAttack", "##colorNmWontAttack"};
        // Rows top to bottom: ordinary mobs, notorious monsters, then mobs with no data.
        const int kRowOrder[kCategoryCount] = {0, 1, 3, 4, 2};
        constexpr float kSliderWidth                      = 240.0f;
    }

    bool Menu::Draw(IGuiManager* gui, Settings& s, const MenuStatus& status)
    {
        if (!open || gui == nullptr) return false;
        bool save = false;

        gui->SetNextWindowSize(ImVec2(420.0f, 0.0f), ImGuiCond_FirstUseEver);
        if (gui->Begin("AggroGlow", &open, ImGuiWindowFlags_AlwaysAutoResize))
        {
            save |= gui->Checkbox("Enable outlines", &s.enabled);
            gui->Separator();

            gui->SetNextItemWidth(kSliderWidth);
            gui->SliderFloat("Thickness", &s.thickness, 1.0f, 16.0f, "%.1f px", ImGuiSliderFlags_AlwaysClamp);
            save |= gui->IsItemDeactivatedAfterEdit();
            gui->SetNextItemWidth(kSliderWidth);
            gui->SliderInt("Smoothness", &s.smoothness, 4, 16, "%d copies", ImGuiSliderFlags_AlwaysClamp);
            save |= gui->IsItemDeactivatedAfterEdit();
            gui->SetNextItemWidth(kSliderWidth);
            gui->SliderFloat("Max distance", &s.maxDistance, 5.0f, 60.0f, "%.0f yalms", ImGuiSliderFlags_AlwaysClamp);
            save |= gui->IsItemDeactivatedAfterEdit();
            gui->Separator();

            for (const int c : kRowOrder)
            {
                save |= gui->Checkbox(kShowIds[c], &s.show[c]);
                gui->SameLine();
                gui->ColorEdit4(kColorIds[c], s.color[c].v, ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoInputs);
                save |= gui->IsItemDeactivatedAfterEdit();
                gui->SameLine();
                gui->TextUnformatted(kCategoryLabels[c]);
            }
            gui->Separator();

            save |= gui->Checkbox("Show level and con above names", &s.showLabels);
            save |= gui->Checkbox("Auto-examine the mob you target", &s.autoExamine);
            gui->BeginDisabled(true);
            gui->TextUnformatted("Checks a targeted mob that gives exp, once per respawn time.");
            gui->TextUnformatted("The checker addon still prints its own line for these checks.");
            gui->EndDisabled();
            gui->Separator();

            char line[128];
            std::snprintf(line, sizeof(line), "%u mobs outlined, %u meshes, %u labels, frame %.2f ms",
                status.outlinedMobs, status.meshes, status.labels, status.frameMs);
            gui->BeginDisabled(true);
            gui->TextUnformatted(line);
            gui->EndDisabled();
            if (!status.stencilAvailable)
                gui->TextUnformatted("Outlines unavailable: the depth buffer has no stencil bits.");
        }
        gui->End();

        s = Clamp(s);
        return save;
    }
}
