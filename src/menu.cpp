#include "menu.h"

#include "Ashita.h"

#include <cstdio>

namespace aggroglow
{
    namespace
    {
        const char* const kCategoryLabels[kCategoryCount] = {"Will attack", "Won't attack", "Unknown (not in MobDB)"};
        const char* const kShowIds[kCategoryCount]        = {"##showWillAttack", "##showWontAttack", "##showUnknown"};
        const char* const kColorIds[kCategoryCount]       = {"##colorWillAttack", "##colorWontAttack", "##colorUnknown"};
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

            for (int c = 0; c < kCategoryCount; ++c)
            {
                save |= gui->Checkbox(kShowIds[c], &s.show[c]);
                gui->SameLine();
                gui->ColorEdit4(kColorIds[c], s.color[c].v, ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoInputs);
                save |= gui->IsItemDeactivatedAfterEdit();
                gui->SameLine();
                gui->TextUnformatted(kCategoryLabels[c]);
            }
            gui->Separator();

            save |= gui->Checkbox("Modern con table (level 99 servers)", &s.modernConTable);
            gui->Separator();

            char line[128];
            std::snprintf(line, sizeof(line), "%u mobs outlined, %u meshes, frame %.2f ms", status.outlinedMobs,
                status.meshes, status.frameMs);
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
