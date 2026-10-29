#pragma once

#include "generated/window_pieces.h"
#include "settings.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>

namespace headsup
{
    // The steps of the Setup window, in order.
    enum class SetupStep : uint8_t { Welcome, Theme, Fonts, MobNameplates, Outlines, TargetCursor, Others, Widgets, MouseExtras };
    constexpr int kSetupSteps = static_cast<int>(SetupStep::MouseExtras) + 1;

    // An option sets every field of its choice, whatever they were, so picking it twice changes nothing more.
    struct SetupOption
    {
        const char* key; // its words' in the window's file
        void (*apply)(Settings&);
    };
    struct SetupChoice
    {
        const char* key;
        WindowWord word; // its name and tip
        WindowList list; // its options' names and tips, in order
        std::span<const SetupOption> options;
    };
    // A step's choices, drawn as options to pick, then its settings with the settings window's own controls.
    struct SetupPage
    {
        SetupStep step;
        const char* key;
        WindowWord word; // its title, and what it is for as the tip
        std::span<const SetupChoice> choices;
        std::span<const SettingPiece> settings;
    };
    std::span<const SetupPage> SetupPages();
    const SetupPage& SetupPageOf(SetupStep step);

    // The option the settings hold, or none when the player set the choice's fields some other way.
    std::optional<size_t> PickedOption(const SetupChoice& choice, const Settings& s);

    // The Widgets step's on and off for each widget, in groups.
    struct SetupWidgetGroup
    {
        const char* key;
        std::span<const BarWidget> widgets; // named as the bar-widgets list names them
    };
    // The groups' names, in order.
    constexpr WindowList kSetupWidgetGroupNames = WindowList::SetupWidgetGroups;
    std::span<const SetupWidgetGroup> SetupWidgetGroups();
    // A widget turned off is off, and its copies leave the panel; turned on, its first copy shows.
    void SetWidgetShown(Settings& s, BarWidget widget, bool shown);

    // The file running Setup again copies the settings to first, beside them: named for the second it was made, with
    // a number after when taken(name) says that name is already a file, so no run writes over another's.
    struct SetupTime
    {
        int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
    };
    std::string SetupBackupName(const SetupTime& at, const std::function<bool(const std::string&)>& taken);
}
