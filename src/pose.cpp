#include "pose.h"

#include "generated/phoenix_rules.h"
#include "nameplate.h"

#include <algorithm>
#include <cmath>
#include <iterator>

namespace headsup
{
    namespace
    {
        constexpr uint32_t kRestingStatus    = 33;
        constexpr uint32_t kSittingStatus    = 47;
        constexpr uint32_t kFirstChairStatus = 63; // the default chair, then one per chair key item
        constexpr uint32_t kLastChairStatus  = 83;

        // Where a posed player's name goes, as a share of the height the game puts it at.
        constexpr float kRestingShare = 0.75f;
        constexpr float kSittingShare = 0.6f;
        constexpr float kChairShare   = 0.7f;

        constexpr float kNearestDepth = 1e-4f; // clip w at or below this is behind the camera
        constexpr float kEndOn        = 1e-6f; // a vertical line seen end on, from straight above or below

        // The clip y and w of the point height above the feet; a row needs only those.
        struct ClipYW
        {
            float y, w;
        };
        ClipYW ClipAt(const Camera& camera, const WorldPoint& feet, float height)
        {
            const float point[4] = {feet.x, feet.y - height, feet.z, 1.0f};
            ClipYW clip{0.0f, 0.0f};
            for (int k = 0; k < 4; ++k)
            {
                clip.y += point[k] * camera.viewProjection[k][1];
                clip.w += point[k] * camera.viewProjection[k][3];
            }
            return clip;
        }
    }

    Pose PoseFromStatus(uint32_t status)
    {
        if (status == kRestingStatus) return Pose::Resting;
        if (status == kSittingStatus) return Pose::Sitting;
        if (status >= kFirstChairStatus && status <= kLastChairStatus) return Pose::Chair;
        return Pose::Standing;
    }

    bool IsSittingStatus(uint32_t status)
    {
        return std::find(std::begin(kSittingAnimations), std::end(kSittingAnimations), status) != std::end(kSittingAnimations);
    }

    WorldPoint FromEntityPosition(float x, float y, float z) { return WorldPoint{x, z, y}; }

    Camera MakeCamera(const float view[16], const float projection[16], float height)
    {
        Camera camera{};
        for (int row = 0; row < 4; ++row)
            for (int col = 0; col < 4; ++col)
                for (int k = 0; k < 4; ++k)
                    camera.viewProjection[row][col] += view[row * 4 + k] * projection[k * 4 + col];
        camera.height = height;
        return camera;
    }

    bool RowAtHeight(const Camera& camera, const WorldPoint& feet, float height, float& row)
    {
        const ClipYW clip = ClipAt(camera, feet, height);
        if (clip.w <= kNearestDepth) return false;
        row = (1.0f - clip.y / clip.w) * 0.5f * camera.height;
        return true;
    }

    bool HeightAtRow(const Camera& camera, const WorldPoint& feet, float row, float& height)
    {
        // A point's clip y and w are linear in its height above the feet, so the height solves directly.
        const ClipYW base = ClipAt(camera, feet, 0.0f);
        const ClipYW one  = ClipAt(camera, feet, 1.0f);
        const float upY = one.y - base.y, upW = one.w - base.w;
        const float ndc    = 1.0f - 2.0f * row / camera.height;
        const float across = upY - ndc * upW;
        if (base.w <= kNearestDepth || std::fabs(across) < kEndOn) return false;
        height = (ndc * base.w - base.y) / across;
        return true;
    }

    float PoseHeightShare(Pose pose)
    {
        switch (pose)
        {
            case Pose::Resting: return kRestingShare;
            case Pose::Sitting: return kSittingShare;
            case Pose::Chair: return kChairShare;
            case Pose::Standing: break;
        }
        return 1.0f;
    }

    float PosedNameRow(const Camera& camera, const WorldPoint& feet, float gameRow, Pose pose)
    {
        float height = 0.0f, row = 0.0f;
        if (pose == Pose::Standing || !HeightAtRow(camera, feet, gameRow, height)) return gameRow;
        return RowAtHeight(camera, feet, height * PoseHeightShare(pose), row) ? row : gameRow;
    }

    ScreenBox PlaceName(const ScreenBox& letters, const ScreenBox* whole, const Camera* camera, const WorldPoint& feet, Pose pose)
    {
        ScreenBox placed = whole != nullptr ? CenteredOver(letters, *whole) : letters;
        if (camera == nullptr) return placed;
        const float lower = PosedNameRow(*camera, feet, placed.maxY, pose) - placed.maxY;
        placed.minY += lower;
        placed.maxY += lower;
        return placed;
    }
}
