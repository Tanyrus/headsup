#include "Ashita.h"

#include "menu.h"
#include "outline.h"
#include "settings.h"
#include "tracker.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>
#include <vector>

namespace
{
    constexpr const char* kName        = "aggroglow";
    constexpr const char* kConfigAlias = "aggroglow";
    constexpr const char* kConfigFile  = "aggroglow/settings.ini"; // relative to Ashita's config folder
    constexpr const char* kSection     = "settings";

    // Settings persistence through Ashita's configuration manager.
    class AshitaStore final : public aggroglow::SettingsStore
    {
    public:
        explicit AshitaStore(IConfigurationManager* config)
            : m_Config(config)
        {}

        bool GetBool(const char* key, bool fallback) override { return m_Config->GetBool(kConfigAlias, kSection, key, fallback); }
        float GetFloat(const char* key, float fallback) override { return m_Config->GetFloat(kConfigAlias, kSection, key, fallback); }
        void Set(const char* key, const char* value) override { m_Config->SetValue(kConfigAlias, kSection, key, value); }

    private:
        IConfigurationManager* m_Config;
    };

    std::vector<std::string> SplitLower(const char* command)
    {
        std::vector<std::string> args;
        std::string current;
        for (const char* p = command; *p; ++p)
        {
            if (*p == ' ' || *p == '\t')
            {
                if (!current.empty()) args.push_back(current), current.clear();
            }
            else
                current += static_cast<char>(std::tolower(static_cast<unsigned char>(*p)));
        }
        if (!current.empty()) args.push_back(current);
        return args;
    }
}

class AggroGlow final : public IPlugin
{
    IAshitaCore* m_AshitaCore = nullptr;
    aggroglow::Settings m_Settings;
    aggroglow::Tracker m_Tracker;
    aggroglow::OutlineRenderer m_Outline;
    aggroglow::Menu m_Menu;
    std::vector<aggroglow::ActorInput> m_Inputs;
    LARGE_INTEGER m_QpcFrequency{};
    LARGE_INTEGER m_LastPresent{};
    double m_FrameMs = 0.0;

public:
    const char* GetName(void) const override { return kName; }
    const char* GetAuthor(void) const override { return "tanyrus"; }
    const char* GetDescription(void) const override { return "Outlines nearby monsters, colored by whether they will attack you."; }
    const char* GetLink(void) const override { return ""; }
    double GetVersion(void) const override { return 1.00; }
    uint32_t GetFlags(void) const override
    {
        return static_cast<uint32_t>(Ashita::PluginFlags::UseCommands | Ashita::PluginFlags::UseDirect3D);
    }

    bool Initialize(IAshitaCore* core, ILogManager* logger, const uint32_t id) override
    {
        UNREFERENCED_PARAMETER(logger);
        UNREFERENCED_PARAMETER(id);
        m_AshitaCore = core;
        QueryPerformanceFrequency(&m_QpcFrequency);
        LoadSettings();
        return true;
    }

    bool Direct3DInitialize(IDirect3DDevice8* device) override
    {
        m_Outline.SetDevice(device);
        return true;
    }

    bool HandleCommand(int32_t mode, const char* command, bool injected) override
    {
        UNREFERENCED_PARAMETER(mode);
        UNREFERENCED_PARAMETER(injected);
        if (command == nullptr) return false;
        const std::vector<std::string> args = SplitLower(command);
        if (args.empty() || (args[0] != "/aggroglow" && args[0] != "/ag")) return false;

        if (args.size() == 1)
            m_Menu.open = !m_Menu.open;
        else if (args[1] == "on" || args[1] == "off")
        {
            m_Settings.enabled = args[1] == "on";
            SaveSettings();
            Print(std::string("outlines ") + (m_Settings.enabled ? "on" : "off"));
        }
        else
        {
            Print("/aggroglow or /ag: open or close the settings window");
            Print("/ag on | /ag off: turn outlines on or off");
        }
        return true;
    }

    void Direct3DPresent(const RECT* source, const RECT* dest, HWND window, const RGNDATA* dirty) override
    {
        UNREFERENCED_PARAMETER(source);
        UNREFERENCED_PARAMETER(dest);
        UNREFERENCED_PARAMETER(window);
        UNREFERENCED_PARAMETER(dirty);
        MeasureFrame();
        m_Outline.NewFrame();
        if (m_Outline.TakeStencilWarning())
            Print("a mob could not be outlined: the game's depth buffer has no stencil bits.");
        UpdateTracker();
        const aggroglow::MenuStatus status{m_Tracker.OutlinedCount(), m_Outline.MeshesLastFrame(), m_FrameMs, m_Outline.StencilAvailable()};
        if (m_Menu.Draw(m_AshitaCore->GetGuiManager(), m_Settings, status))
            SaveSettings();
    }

    bool Direct3DDrawIndexedPrimitive(D3DPRIMITIVETYPE type, UINT minIndex, UINT numVertices, UINT startIndex, UINT primCount) override
    {
        return m_Outline.OnDrawIndexed(type, minIndex, numVertices, startIndex, primCount, m_Tracker, m_Settings);
    }

private:
    void Print(const std::string& message)
    {
        const std::string line = Ashita::Chat::Header(kName) + Ashita::Chat::Message(message);
        m_AshitaCore->GetChatManager()->Write(1, false, line.c_str());
    }

    void LoadSettings()
    {
        IConfigurationManager* config = m_AshitaCore->GetConfigurationManager();
        config->Load(kConfigAlias, kConfigFile); // a missing file leaves every value at its default
        AshitaStore store(config);
        m_Settings = aggroglow::LoadSettings(store);
    }

    void SaveSettings()
    {
        const std::string folder = std::string(m_AshitaCore->GetInstallPath()) + "config\\aggroglow";
        CreateDirectoryA(folder.c_str(), nullptr);
        IConfigurationManager* config = m_AshitaCore->GetConfigurationManager();
        AshitaStore store(config);
        aggroglow::SaveSettings(m_Settings, store);
        if (!config->Save(kConfigAlias, kConfigFile))
            Print("could not save config/aggroglow/settings.ini");
    }

    void MeasureFrame()
    {
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        if (m_LastPresent.QuadPart != 0 && m_QpcFrequency.QuadPart != 0)
        {
            const double ms = static_cast<double>(now.QuadPart - m_LastPresent.QuadPart) * 1000.0 / static_cast<double>(m_QpcFrequency.QuadPart);
            m_FrameMs       = m_FrameMs == 0.0 ? ms : m_FrameMs * 0.95 + ms * 0.05;
        }
        m_LastPresent = now;
    }

    // Snapshot every entity with an actor, then let the tracker decide which mobs get outlined next frame.
    void UpdateTracker()
    {
        IEntity* entity = m_AshitaCore->GetMemoryManager()->GetEntity();
        IParty* party   = m_AshitaCore->GetMemoryManager()->GetParty();
        IPlayer* player = m_AshitaCore->GetMemoryManager()->GetPlayer();
        m_Inputs.clear();
        const uint32_t count = std::min<uint32_t>(entity->GetEntityMapSize(), 4096);
        for (uint32_t i = 0; i < count; ++i)
        {
            if (entity->GetRawEntity(i) == nullptr) continue;
            const uintptr_t actor = entity->GetActorPointer(i);
            if (actor == 0) continue;
            const char* name = entity->GetName(i);
            m_Inputs.push_back(aggroglow::ActorInput{static_cast<aggroglow::ActorPtr>(actor), static_cast<uint16_t>(i),
                (entity->GetSpawnFlags(i) & 0x10) != 0, entity->GetHPPercent(i) > 0,
                std::sqrt(std::max(0.0f, entity->GetDistance(i))), name != nullptr ? name : ""});
        }
        aggroglow::PlayerState state;
        state.level   = player->GetMainJobLevel();
        state.sitting = aggroglow::IsSittingStatus(entity->GetStatus(party->GetMemberTargetIndex(0)));
        m_Tracker.Update(m_Inputs, party->GetMemberZone(0), state, m_Settings);
    }
};

extern "C" __declspec(dllexport) double __stdcall expGetInterfaceVersion(void)
{
    return ASHITA_INTERFACE_VERSION;
}

extern "C" __declspec(dllexport) IPlugin* __stdcall expCreatePlugin(const char* args)
{
    UNREFERENCED_PARAMETER(args);
    return new AggroGlow();
}

extern "C" __declspec(dllexport) void __stdcall expDestroyPlugin(void* instance)
{
    delete static_cast<IPlugin*>(instance);
}
