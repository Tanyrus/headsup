#include "setup_preview.h"

#include "outline_math.h"

#include <array>
#include <cmath>

namespace headsup
{
    namespace
    {
        constexpr uint32_t kBackground = 0xFF20242C;
        constexpr float kBesideUnits   = 1.3f;  // of the models' own space between them standing side by side
        constexpr float kFacingYaw     = 1.5707963f; // radians round the scene to its front: the game's models face +x
        constexpr float kSwayYaw       = 0.35f, kSwaySpeed = 0.4f; // a slow turn back and forth, so its shape shows
        constexpr float kHeadroom      = 0.6f; // of the models' height, framed over them for the plates and the cursor
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

    bool SetupPreview::Render(const Settings& settings, SetupStep step, int zoom, double now, const std::string& yourName,
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
            // Seen from the front, across the picture is z: you on its left, Shantotto on its right.
            placed.push_back({&m_You.model, SampleActor::You, {0.0f, 0.0f, kBesideUnits}});
            placed.push_back({&m_Shantotto.model, SampleActor::Shantotto, {0.0f, 0.0f, -kBesideUnits}});
        }
        ModelBounds box = Moved(placed.front().model->Standing(), placed.front().at);
        for (const Placed& p : placed)
            box = Joined(box, Moved(p.model->Standing(), p.at));
        const float headroom = (box.high.y - box.low.y) * kHeadroom;
        // Zoomed toward halfway up the room over the heads, where the plates are, so they stay in the picture as
        // they grow. The game's up is -y.
        Vec3 toward = HeadOf(box);
        toward.y -= headroom / 2.0f;
        box.low.y -= headroom;
        const float closer   = static_cast<float>(zoom) / kPercent;
        const float yaw      = kFacingYaw + kSwayYaw * static_cast<float>(std::sin(now * kSwaySpeed));
        const ModelView view = FramedView(box, yaw, static_cast<float>(kWidth) / static_cast<float>(kHeight), closer, toward);

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

        // The plates sit over each model's head where the camera sees it, at the size of a name drawn whole. Zoomed,
        // they are laid out on a screen a zoomth of the picture's size and drawn zoom times as large.
        const PlateScene plates{static_cast<float>(kWidth) / closer, static_cast<float>(kHeight) / closer, nullptr,
            [&](uint16_t index, bool) -> std::optional<DrawnName> {
                for (const Placed& p : placed)
                    if (SampleIndex(p.actor) == index)
                        if (const auto head = PointInPicture(view, HeadOf(Moved(p.model->Standing(), p.at)), kWidth, kHeight))
                            return SampleName(head->x / closer, head->y / closer);
                return std::nullopt;
            }};
        m_Plates.Update(m_Tracker, plates, settings, closer, closer, scene.targets, now, SampleIndex(SampleActor::You), {});
        m_Plates.Draw(false);
        Opaque();
        m_Drawn = true;
        return true;
    }

    void SetupPreview::Opaque()
    {
        // What was blended over the background left alphas under one, which would let the game show through the
        // window: every pixel's alpha alone is set whole.
        struct Corner
        {
            float x, y, z, rhw;
            D3DCOLOR color;
        };
        const auto w = static_cast<float>(kWidth), h = static_cast<float>(kHeight);
        const Corner corners[]{{0, 0, 0, 1, 0xFFFFFFFF}, {w, 0, 0, 1, 0xFFFFFFFF}, {0, h, 0, 1, 0xFFFFFFFF}, {w, h, 0, 1, 0xFFFFFFFF}};
        m_Device->SetVertexShader(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
        m_Device->SetTexture(0, nullptr);
        m_Device->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
        m_Device->SetRenderState(D3DRS_STENCILENABLE, FALSE);
        m_Device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        m_Device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
        m_Device->SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_ALPHA);
        m_Device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
        m_Device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
        m_Device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
        m_Device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
        m_Device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, corners, sizeof(Corner));
        m_Device->SetRenderState(D3DRS_COLORWRITEENABLE,
            D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_ALPHA);
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
