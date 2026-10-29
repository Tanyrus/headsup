#include "model_pose.h"
#include "test.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

using namespace headsup;

namespace
{
    constexpr float kHalfRoot = 0.70710678f; // sin and cos of 45 degrees: a quarter turn's quaternion
    constexpr Quat kQuarterAboutZ{0, 0, kHalfRoot, kHalfRoot};
    constexpr Quat kQuarterAboutX{kHalfRoot, 0, 0, kHalfRoot};

    bool Near(float a, float b) { return std::fabs(a - b) < 1e-4f; }
    bool At(const Mat4& m, float x, float y, float z) { return Near(m.m[12], x) && Near(m.m[13], y) && Near(m.m[14], z); }
    bool At(const DrawVertex& v, float x, float y, float z) { return Near(v.x, x) && Near(v.y, y) && Near(v.z, z); }

    // A root turned a quarter about z, and a joint one up from it: which the turn puts one along -x.
    std::vector<Joint> Arm() { return {{-1, kQuarterAboutZ, {}}, {0, {}, {0, 1, 0}}}; }

    JointTrack Still(int joint)
    {
        JointTrack track;
        track.joint = joint;
        const float still[JointTrack::kChannels]{0, 0, 0, 1, 0, 0, 0, 1, 1, 1};
        for (int c = 0; c < JointTrack::kChannels; ++c)
            track.channels[static_cast<size_t>(c)] = {still[c]};
        return track;
    }
    Motion OneTrack(JointTrack track, int frames = 1)
    {
        Motion motion;
        motion.frames = frames;
        motion.speed  = 1.0f;
        motion.tracks = {std::move(track)};
        return motion;
    }

    ModelVertex OneJoint(uint8_t joint, Vec3 at, Vec3 normal = {0, 0, 1})
    {
        ModelVertex v;
        v.joints       = {joint, 0};
        v.mirrorJoints = {joint, 0};
        v.positions    = {at, {}};
        v.normal       = normal;
        return v;
    }
    ModelPart Part(const char* picture, std::vector<ModelTriangle> triangles)
    {
        ModelPart part;
        part.picture   = picture;
        part.triangles = std::move(triangles);
        return part;
    }
}

TEST(a_joint_stands_where_its_parents_turn_and_place_put_it)
{
    const auto joints = PoseSkeleton(Arm(), {}, 0.0f);
    CHECK_EQ(joints.size(), 2u);
    CHECK(At(joints[0], 0, 0, 0));
    CHECK(At(joints[1], -1, 0, 0));
}

TEST(a_motions_move_is_added_to_the_joints_own_place)
{
    JointTrack track = Still(1);
    track.channels[6] = {2.0f};
    const Motion motion = OneTrack(track);
    const Motion* motions[]{&motion};
    CHECK(At(PoseSkeleton(Arm(), motions, 0.0f)[1], -1, 0, 2));
}

TEST(a_motions_turn_comes_after_the_joints_own)
{
    // Turned a quarter about x and then about z, one up lands one along z; the other order would leave it along -x.
    const std::vector<Joint> skeleton{{-1, kQuarterAboutX, {}}, {0, {}, {0, 1, 0}}};
    JointTrack track = Still(0);
    track.channels[2] = {kHalfRoot};
    track.channels[3] = {kHalfRoot};
    const Motion motion = OneTrack(track);
    const Motion* motions[]{&motion};
    CHECK(At(PoseSkeleton(skeleton, motions, 0.0f)[1], 0, 0, 1));
}

TEST(a_frame_between_two_blends_them)
{
    const std::vector<Joint> skeleton{{-1, {}, {}}, {0, {}, {0, 1, 0}}};
    JointTrack track  = Still(1);
    track.channels[4] = {0.0f, 4.0f};
    const Motion motion = OneTrack(track, 2);
    const Motion* motions[]{&motion};
    CHECK(At(PoseSkeleton(skeleton, motions, 0.5f)[1], 2, 1, 0));
    CHECK(At(PoseSkeleton(skeleton, motions, 1.0f)[1], 4, 1, 0));
}

TEST(a_joints_size_carries_to_the_joints_under_it)
{
    const std::vector<Joint> skeleton{{-1, {}, {}}, {0, {}, {0, 1, 0}}, {1, {}, {0, 1, 0}}};
    JointTrack track = Still(1);
    track.channels[7] = track.channels[8] = track.channels[9] = {2.0f};
    const Motion motion = OneTrack(track);
    const Motion* motions[]{&motion};
    CHECK(At(PoseSkeleton(skeleton, motions, 0.0f)[2], 0, 3, 0));
}

TEST(a_later_motion_wins_a_joint_both_move)
{
    JointTrack first = Still(1), second = Still(1);
    first.channels[6]  = {5.0f};
    second.channels[6] = {2.0f};
    const Motion a = OneTrack(first), b = OneTrack(second);
    const Motion* motions[]{&a, &b};
    CHECK(At(PoseSkeleton(Arm(), motions, 0.0f)[1], -1, 0, 2));
}

TEST(a_motion_plays_at_its_speed_in_frames_a_30th_of_a_second_and_loops)
{
    Motion idle;
    idle.frames = 16;
    idle.speed  = 0.5f;
    CHECK(Near(MotionFrame(idle, 1.0), 15.0f));
    CHECK(Near(MotionFrame(idle, 2.0), 14.0f));
    idle.frames = 0;
    CHECK(Near(MotionFrame(idle, 2.0), 0.0f));
}

TEST(a_skinned_vertex_is_its_joints_places_of_it_weighted)
{
    ModelVertex two;
    two.twoJoints = true;
    two.joints    = {0, 1};
    two.positions = {Vec3{0, 0, 1}, Vec3{0, 0, 1}};
    two.weight    = 0.25f;
    ModelMesh mesh;
    mesh.vertices = {OneJoint(1, {1, 0, 0}, {1, 0, 0}), two, OneJoint(0, {})};
    mesh.parts    = {Part("tim     skin", {{{0, 1, 2}, {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f}}})};
    const ModelMesh meshes[]{mesh};
    const auto batches = PosedBatches(meshes, PoseSkeleton(Arm(), {}, 0.0f));
    CHECK_EQ(batches.size(), 1u);
    CHECK_EQ(batches[0].picture, std::string("tim     skin"));
    const auto& v = batches[0].vertices;
    CHECK_EQ(v.size(), 3u);
    CHECK(At(v[0], -1, 1, 0));
    CHECK(Near(v[0].nx, 0) && Near(v[0].ny, 1) && Near(v[0].nz, 0));
    CHECK(At(v[1], -0.75f, 0, 2));
    CHECK(Near(v[1].u, 0.3f) && Near(v[1].v, 0.4f));
    CHECK(At(v[2], 0, 0, 0));
}

TEST(a_mirrored_mesh_is_drawn_again_by_its_mirror_joints_turned_round_its_axis)
{
    ModelVertex flipped = OneJoint(1, {1, 0, 0});
    flipped.mirrorJoints = {0, 0};
    flipped.mirrorAxes   = {1, 0};
    ModelMesh mesh;
    mesh.mirrored = true;
    mesh.vertices = {flipped, OneJoint(0, {0, 0, 1}), OneJoint(0, {0, 2, 0})};
    mesh.parts    = {Part("tim     coat", {{{0, 1, 2}, {}}})};
    const ModelMesh meshes[]{mesh};
    const auto v = PosedBatches(meshes, PoseSkeleton(Arm(), {}, 0.0f)).at(0).vertices;
    CHECK_EQ(v.size(), 6u);
    CHECK(At(v[0], -1, 1, 0));
    CHECK(At(v[3], 0, -1, 0)); // x turned round, then the root's quarter turn
    CHECK(At(v[4], -2, 0, 0)); // wound the other way: the third corner second
    CHECK(At(v[5], 0, 0, 1));
}

TEST(meshes_that_share_a_picture_share_its_batch)
{
    ModelMesh head, hair;
    head.vertices = hair.vertices = {OneJoint(0, {}), OneJoint(0, {}), OneJoint(0, {})};
    head.parts    = {Part("tim     face", {{{0, 1, 2}, {}}}), Part("tim     hair", {{{0, 1, 2}, {}}})};
    hair.parts    = {Part("tim     face", {{{0, 1, 2}, {}}})};
    const ModelMesh meshes[]{head, hair};
    const auto batches = PosedBatches(meshes, PoseSkeleton(Arm(), {}, 0.0f));
    CHECK_EQ(batches.size(), 2u);
    CHECK_EQ(batches[0].vertices.size(), 6u);
    CHECK_EQ(batches[1].picture, std::string("tim     hair"));
}

TEST(an_untextured_part_draws_each_triangle_in_its_own_color)
{
    ModelMesh mesh;
    mesh.vertices = {OneJoint(0, {}), OneJoint(0, {}), OneJoint(0, {})};
    ModelPart part = Part("", {{{0, 1, 2}, {}}, {{2, 1, 0}, {}}});
    part.textured  = false;
    part.colors    = {0xFF102030, 0xFF405060};
    mesh.parts     = {part};
    const ModelMesh meshes[]{mesh};
    const DrawBatch batch = PosedBatches(meshes, PoseSkeleton(Arm(), {}, 0.0f)).at(0);
    CHECK(batch.picture.empty());
    CHECK_EQ(batch.vertices[0].color, 0xFF102030u);
    CHECK_EQ(batch.vertices[5].color, 0xFF405060u);
}

TEST(a_vertex_naming_a_joint_the_skeleton_lacks_is_left_out_with_its_triangles)
{
    ModelMesh mesh;
    mesh.vertices = {OneJoint(0, {}), OneJoint(9, {}), OneJoint(0, {})};
    mesh.parts    = {Part("tim     skin", {{{0, 1, 2}, {}}, {{0, 2, 0}, {}}})};
    const ModelMesh meshes[]{mesh};
    CHECK_EQ(PosedBatches(meshes, PoseSkeleton(Arm(), {}, 0.0f)).at(0).vertices.size(), 3u);
}

namespace
{
    // Where a point lands in the picture, -1 to 1 across and up, and its depth 0 to 1.
    std::array<float, 3> Projected(const ModelView& view, float x, float y, float z)
    {
        const auto times = [](const std::array<float, 4>& p, const Mat4& m) {
            std::array<float, 4> out{};
            for (size_t c = 0; c < 4; ++c)
                for (size_t k = 0; k < 4; ++k)
                    out[c] += p[k] * m.m[k * 4 + c];
            return out;
        };
        const auto clip = times(times({x, y, z, 1}, view.view), view.projection);
        return {clip[0] / clip[3], clip[1] / clip[3], clip[2] / clip[3]};
    }
}

TEST(a_framed_view_shows_the_whole_box_head_up_from_every_side_and_shape)
{
    // The Tonberry's box: the game's up is -y, so its head is at the low y.
    const ModelBounds box{{-0.746f, -0.973f, -0.422f}, {0.606f, 0.001f, 0.652f}};
    for (float yaw : {0.0f, 1.0f, 2.5f, -2.0f})
        for (float aspect : {0.5f, 1.0f, 2.0f})
        {
            const ModelView view = FramedView(box, yaw, aspect);
            float widest = 0.0f;
            for (int corner = 0; corner < 8; ++corner)
            {
                const auto p = Projected(view, corner & 1 ? box.high.x : box.low.x, corner & 2 ? box.high.y : box.low.y,
                    corner & 4 ? box.high.z : box.low.z);
                CHECK(std::fabs(p[0]) <= 1.0f && std::fabs(p[1]) <= 1.0f && p[2] > 0.0f && p[2] < 1.0f);
                widest = std::max({widest, std::fabs(p[0]), std::fabs(p[1])});
            }
            CHECK(widest > 0.5f); // near enough to fill much of the picture
            const float middleX = (box.low.x + box.high.x) / 2, middleZ = (box.low.z + box.high.z) / 2;
            const auto middle = Projected(view, middleX, (box.low.y + box.high.y) / 2, middleZ);
            CHECK(Near(middle[0], 0.0f) && Near(middle[1], 0.0f));
            CHECK(Projected(view, middleX, box.low.y, middleZ)[1] > Projected(view, middleX, box.high.y, middleZ)[1]);
        }
}

TEST(a_turned_view_sees_the_box_from_another_side)
{
    const ModelBounds box{{-1, -2, -1}, {1, 0, 1}};
    // A point on the box's +x side is to one side of the middle seen from the front, and nearer than it seen from +x.
    const auto front = Projected(FramedView(box, 0.0f, 1.0f), 1, -1, 0);
    const auto side  = Projected(FramedView(box, 1.5707963f, 1.0f), 1, -1, 0);
    const auto sideMiddle = Projected(FramedView(box, 1.5707963f, 1.0f), 0, -1, 0);
    CHECK(std::fabs(front[0]) > 0.1f);
    CHECK(Near(side[0], 0.0f) && side[2] < sideMiddle[2]);
}

TEST(the_box_of_posed_batches_holds_every_corner)
{
    std::vector<DrawBatch> batches(2);
    batches[0].vertices = {{1, -2, 3}, {-1, 0, 0}};
    batches[1].vertices = {{0, 5, -4}};
    const ModelBounds box = BoundsOf(batches);
    CHECK(Near(box.low.x, -1) && Near(box.low.y, -2) && Near(box.low.z, -4));
    CHECK(Near(box.high.x, 1) && Near(box.high.y, 5) && Near(box.high.z, 3));
}
