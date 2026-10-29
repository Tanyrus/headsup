#include "setup_preview.h"

#include "outline_math.h"

#include <array>
#include <cmath>

namespace headsup
{
    namespace
    {
        constexpr uint32_t kBackground = 0xFF20242C;
        constexpr float kBeside        = 1.3f;  // model units between the scene's models standing side by side
        constexpr float kFacingYaw     = 0.0f;  // radians the camera turns round the scene from its front
        constexpr float kSwayYaw       = 0.35f, kSwaySpeed = 0.4f; // a slow turn back and forth, so its shape shows
    }

    void SetupPreview::SetDevice(IDirect3DDevice8* device)
    {
        m_Device = device;
        m_Plates.SetDevice(device);
    }

    bool SetupPreview::Ready(Sample& sample, const std::function<std::optional<ModelFile>(std::string&)>& read)
    {
        if (sample.model.Loaded()) return true;
        if (std::exchange(sample.tried, true)) return false;
        std::string failure;
        std::optional<ModelFile> model = read(failure);
        if (!model)
        {
            m_Failure = failure;
            return false;
        }
        if (!sample.model.Load(m_Device, std::move(*model)))
        {
            m_Failure = "Direct3D could not make a model's pictures";
            return false;
        }
        return true;
    }

    bool SetupPreview::Render(const Settings& settings, SetupStep step, double now, const std::string& yourName,
        const PlayerLook& yourLook, const Readers& read)
    {
        m_Drawn = false;
        if (m_Device == nullptr) return false;
        const bool others = step == SetupStep::Others;
        if (others && m_You.look != yourLook)
        {
            m_You.model.Release();
            m_You.tried = false;
            m_You.look  = yourLook;
        }
        const bool ready = Ready(m_Tonberry, [&](std::string& f) { return read.npc(kSampleTonberryModel, f); }) &&
                           (!others || (Ready(m_You, [&](std::string& f) { return read.player(yourLook, f); }) &&
                                           Ready(m_Shantotto, [&](std::string& f) { return read.player(kShantottoLook, f); })));
        if (!ready || !m_Picture.Begin(m_Device, kWidth, kHeight, kBackground)) return false;

        struct Placed
        {
            const DrawnModel* model;
            SampleActor actor;
            Vec3 at;
        };
        std::vector<Placed> placed{{&m_Tonberry.model, SampleActor::Tonberry, {}}};
        if (others)
        {
            placed.push_back({&m_You.model, SampleActor::You, {-kBeside, 0.0f, 0.0f}});
            placed.push_back({&m_Shantotto.model, SampleActor::Shantotto, {kBeside, 0.0f, 0.0f}});
        }
        ModelBounds box = Moved(placed.front().model->Standing(), placed.front().at);
        for (const Placed& p : placed)
            box = Joined(box, Moved(p.model->Standing(), p.at));
        const float yaw      = kFacingYaw + kSwayYaw * static_cast<float>(std::sin(now * kSwaySpeed));
        const ModelView view = FramedView(box, yaw, static_cast<float>(kWidth) / static_cast<float>(kHeight));

        const SampleScene scene = SetupScene(others, yourName);
        m_Tracker.Update(scene.actors, scene.player, settings, scene.targets);
        for (const Placed& p : placed)
        {
            const ActorInfo* info = m_Tracker.FindIndex(SampleIndex(p.actor));
            std::optional<ModelOutline> outline;
            if (info != nullptr && info->outline)
                outline = ModelOutline{info->argb, info->outlineAlpha, OutlinePixels(settings.outlineThickness), settings.smoothness,
                    static_cast<uint8_t>(static_cast<int>(p.actor) + 1)};
            p.model->Draw(m_Device, p.model->Posed(now, p.at), view, yaw, outline ? &*outline : nullptr, kWidth, kHeight);
        }

        // The plates sit over each model's head where the camera sees it, at the size of a name drawn whole.
        const PlateScene plates{static_cast<float>(kWidth), static_cast<float>(kHeight), nullptr,
            [&](uint16_t index, bool) -> std::optional<DrawnName> {
                for (const Placed& p : placed)
                    if (SampleIndex(p.actor) == index)
                        if (const auto head = PointInPicture(view, HeadOf(Moved(p.model->Standing(), p.at)), kWidth, kHeight))
                            return SampleName(head->x, head->y);
                return std::nullopt;
            }};
        m_Plates.Update(m_Tracker, plates, settings, 1.0f, 1.0f, scene.targets, now, SampleIndex(SampleActor::You), {});
        m_Plates.Draw(false);
        m_Drawn = true;
        return true;
    }

    std::optional<RmlRender::LivePicture> SetupPreview::Picture() const
    {
        if (!m_Drawn || m_Picture.Texture() == nullptr) return std::nullopt;
        return RmlRender::LivePicture{m_Picture.Texture(), m_Picture.U(), m_Picture.V()};
    }

    void SetupPreview::Release()
    {
        m_Picture.Release();
        for (Sample* sample : {&m_Tonberry, &m_You, &m_Shantotto})
        {
            sample->model.Release();
            sample->tried = false;
        }
        m_Plates.Release();
        m_Drawn = false;
    }
}
