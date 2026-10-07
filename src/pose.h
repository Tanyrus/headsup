#pragma once

#include "nameplate.h"

#include <cstdint>

namespace headsup
{
    // How a player is posed. The game leaves a name at standing height whatever the pose, and lifts it by the chair's
    // height in a chair, so it floats well above a lowered head.
    enum class Pose
    {
        Standing,
        Resting, // /heal
        Sitting, // /sit
        Chair,   // /sitchair
    };
    Pose PoseFromStatus(uint32_t status);

    // A point in the game's world in Direct3D's axes: x, height (up is negative) and the entity's y.
    struct WorldPoint
    {
        float x, y, z;
    };
    WorldPoint FromEntityPosition(float x, float y, float z);

    // The scene camera: world points to back-buffer pixels.
    struct Camera
    {
        float viewProjection[4][4];
        float width, height;
    };
    Camera MakeCamera(const float view[16], const float projection[16], float width, float height);

    // The back-buffer row of the point height above the feet, and the height whose point is on a row; false when it
    // cannot be projected (behind the camera, or the vertical seen end on).
    bool RowAtHeight(const Camera& camera, const WorldPoint& feet, float height, float& row);
    bool HeightAtRow(const Camera& camera, const WorldPoint& feet, float row, float& height);

    // Where a posed player's name goes, as a share of the height the game puts it at; 1 standing.
    float PoseHeightShare(Pose pose);

    // The back-buffer row for a posed player's name, from the row the game puts it on: its height above their feet,
    // scaled by PoseHeightShare. The game's row when the feet cannot be projected.
    float PosedNameRow(const Camera& camera, const WorldPoint& feet, float gameRow, Pose pose);

    // Where our name goes, from the game's letters: centered over the whole nameplate (CenteredOver; nullptr without
    // one) and, for a posed player, brought down to their head (PosedNameRow; not without a camera).
    ScreenBox PlaceName(const ScreenBox& letters, const ScreenBox* whole, const Camera* camera, const WorldPoint& feet, Pose pose);
}
