#pragma once

#include <cstdint>

namespace headsup
{
    // The game leaves a name at standing height whatever the pose, and lifts it by the chair's height in a chair, so it
    // floats well above a lowered head.
    enum class Pose
    {
        Standing,
        Resting, // /heal
        Sitting, // /sit
        Chair,   // /sitchair
    };
    Pose PoseFromStatus(uint32_t status);

    // Phoenix's CBattleEntity::isSitting(), which lets Too Weak aggressive mobs aggro, with the animations its map
    // server counts.
    bool IsSittingStatus(uint32_t status);

    // A point in the game's world in Direct3D's axes: x, height (up is negative) and the entity's y.
    struct WorldPoint
    {
        float x, y, z;
    };
    WorldPoint FromEntityPosition(float x, float y, float z);

    struct Camera
    {
        float viewProjection[4][4];
        float screenHeight;
    };
    Camera MakeCamera(const float view[16], const float projection[16], float screenHeight);

    bool RowAtHeight(const Camera& camera, const WorldPoint& feet, float height, float& row);
    bool HeightAtRow(const Camera& camera, const WorldPoint& feet, float row, float& height);

    float PoseHeightShare(Pose pose);
    float PosedNameRow(const Camera& camera, const WorldPoint& feet, float gameRow, Pose pose);
}
