#pragma once

#include "model_file.h"
#include "outline_math.h"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace headsup
{
    // Each joint's place in the model's space, posed at a frame of the motions (each moves the joints its tracks
    // name; a later one wins) or as the model stands with none. Row vectors, as D3DMATRIX: a point p of a joint is at
    // p * matrix. The animation's turn comes before the joint's own and its move is added to it, as the game plays
    // them; a frame between two blends them.
    std::vector<Mat4> PoseSkeleton(std::span<const Joint> skeleton, std::span<const Motion* const> motions, float frame);

    // The frame a motion is at after seconds, looping.
    float MotionFrame(const Motion& motion, double seconds);

    struct DrawVertex
    {
        float x = 0.0f, y = 0.0f, z = 0.0f;
        float nx = 0.0f, ny = 0.0f, nz = 0.0f;
        uint32_t color = 0xFFFFFFFF; // ARGB
        float u = 0.0f, v = 0.0f;
    };
    // Triangles drawn with one picture, three vertices each; picture is empty for those in their own colors.
    struct DrawBatch
    {
        std::string picture;
        std::vector<DrawVertex> vertices;
    };
    // The meshes posed by the joints' places, gathered by picture in the order each is first used; a mirrored mesh
    // is drawn twice, the second time turned around its mirror axes and wound the other way.
    std::vector<DrawBatch> PosedBatches(std::span<const ModelMesh> meshes, std::span<const Mat4> joints);

    // The box a posed model fills.
    struct ModelBounds
    {
        Vec3 low, high;
    };
    ModelBounds BoundsOf(std::span<const DrawBatch> batches);

    // A camera on level with a box's middle, turned yaw radians round it about the game's up (which is -y), as near
    // as lets the whole box show in a picture aspect times as wide as it is tall. Left-handed, row vectors. Zoomed, it
    // comes zoom times nearer, toward a point that keeps its place in the picture.
    struct ModelView
    {
        Mat4 view, projection;
    };
    ModelView FramedView(const ModelBounds& box, float yaw, float aspect, float zoom = 1.0f, std::optional<Vec3> toward = std::nullopt);

    // The picture moved across and down by shares of its width and height, as a drag moves it.
    void Pan(ModelView& view, float across, float down);

    // Models stood side by side: their triangles and boxes moved, and the box that holds two.
    void Move(std::vector<DrawBatch>& batches, const Vec3& by);
    ModelBounds Moved(const ModelBounds& box, const Vec3& by);
    ModelBounds Joined(const ModelBounds& a, const ModelBounds& b);
    // The top of a box's middle, where a name goes over a model: the game's up is -y.
    Vec3 HeadOf(const ModelBounds& box);
    // Where a point lands in a picture width by height seen through a view, in pixels from its top left; none for a
    // point behind the camera.
    struct PicturePoint
    {
        float x = 0.0f, y = 0.0f;
    };
    std::optional<PicturePoint> PointInPicture(const ModelView& view, const Vec3& point, float width, float height);
}
