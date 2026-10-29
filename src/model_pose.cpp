#include "model_pose.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace headsup
{
    namespace
    {
        constexpr double kTicksPerSecond = 30.0; // the game's frames are counted in 30ths of a second
        constexpr float kFieldOfView     = 0.5f;  // radians, up and down
        constexpr float kDepthRoom       = 1.5f;  // the near and far planes this many box radii from the middle
        constexpr float kNearest         = 0.01f;
        enum Channel : int { kTurnX, kTurnY, kTurnZ, kTurnW, kMoveX, kMoveY, kMoveZ, kSizeX, kSizeY, kSizeZ };
        constexpr int kM11 = 0, kM12 = 1, kM13 = 2, kM21 = 4, kM22 = 5, kM23 = 6, kM31 = 8, kM32 = 9, kM33 = 10;
        constexpr int kM34 = 11, kM41 = 12, kM42 = 13, kM43 = 14, kM44 = 15;

        Quat Times(const Quat& a, const Quat& b)
        {
            return {a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y, a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
                a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w, a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z};
        }

        // A joint's own space in its parent's, row vectors: sized, then turned, then moved.
        Mat4 Local(const Quat& q, const Vec3& t, const Vec3& s)
        {
            const float n     = q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w;
            const float k     = n > 0.0f ? 2.0f / n : 0.0f;
            const float xx = q.x * q.x * k, yy = q.y * q.y * k, zz = q.z * q.z * k;
            const float xy = q.x * q.y * k, xz = q.x * q.z * k, yz = q.y * q.z * k;
            const float wx = q.w * q.x * k, wy = q.w * q.y * k, wz = q.w * q.z * k;
            Mat4 m{};
            m.m[kM11] = (1 - yy - zz) * s.x, m.m[kM12] = (xy + wz) * s.x, m.m[kM13] = (xz - wy) * s.x;
            m.m[kM21] = (xy - wz) * s.y, m.m[kM22] = (1 - xx - zz) * s.y, m.m[kM23] = (yz + wx) * s.y;
            m.m[kM31] = (xz + wy) * s.z, m.m[kM32] = (yz - wx) * s.z, m.m[kM33] = (1 - xx - yy) * s.z;
            m.m[kM41] = t.x, m.m[kM42] = t.y, m.m[kM43] = t.z, m.m[kM44] = 1;
            return m;
        }

        constexpr int kSide = 4; // a Mat4's rows and columns

        // A row vector by a matrix.
        std::array<float, kSide> RowTimes(const std::array<float, kSide>& p, const Mat4& m)
        {
            std::array<float, kSide> out{};
            for (int c = 0; c < kSide; ++c)
                for (int k = 0; k < kSide; ++k)
                    out[static_cast<size_t>(c)] += p[static_cast<size_t>(k)] * m.m[k * kSide + c];
            return out;
        }

        Mat4 Times(const Mat4& a, const Mat4& b)
        {
            Mat4 out{};
            for (int r = 0; r < kSide; ++r)
                for (int c = 0; c < kSide; ++c)
                    for (int k = 0; k < kSide; ++k)
                        out.m[r * kSide + c] += a.m[r * kSide + k] * b.m[k * kSide + c];
            return out;
        }

        // A point's place by a matrix, its move counted weight times; a direction's turn alone.
        Vec3 Place(const Vec3& p, const Mat4& m, float weight)
        {
            return {p.x * m.m[kM11] + p.y * m.m[kM21] + p.z * m.m[kM31] + weight * m.m[kM41],
                p.x * m.m[kM12] + p.y * m.m[kM22] + p.z * m.m[kM32] + weight * m.m[kM42],
                p.x * m.m[kM13] + p.y * m.m[kM23] + p.z * m.m[kM33] + weight * m.m[kM43]};
        }
        Vec3 Plus(const Vec3& a, const Vec3& b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
        Vec3 Unit(const Vec3& v)
        {
            const float length = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
            return length > 0.0f ? Vec3{v.x / length, v.y / length, v.z / length} : v;
        }
        Vec3 TurnedRound(Vec3 v, MirrorAxis axis)
        {
            if (axis == MirrorAxis::X) v.x = -v.x;
            if (axis == MirrorAxis::Y) v.y = -v.y;
            if (axis == MirrorAxis::Z) v.z = -v.z;
            return v;
        }

        // A channel's number between two frames, the second the first again past the last.
        struct Between
        {
            int first = 0, second = 0;
            float share = 0.0f;
        };
        Between FramesAround(const Motion& motion, float frame)
        {
            if (motion.frames <= 1) return {};
            const float whole = std::floor(frame);
            const int first   = static_cast<int>(whole) % motion.frames;
            return {first, (first + 1) % motion.frames, frame - whole};
        }
        float Blend(const JointTrack& track, int channel, const Between& at)
        {
            return track.At(channel, at.first) * (1.0f - at.share) + track.At(channel, at.second) * at.share;
        }
        Quat BlendTurn(const JointTrack& track, const Between& at)
        {
            Quat a{track.At(kTurnX, at.first), track.At(kTurnY, at.first), track.At(kTurnZ, at.first), track.At(kTurnW, at.first)};
            Quat b{track.At(kTurnX, at.second), track.At(kTurnY, at.second), track.At(kTurnZ, at.second), track.At(kTurnW, at.second)};
            // The same turn either way round: blend the nearer.
            const float sign = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w < 0.0f ? -1.0f : 1.0f;
            const float s    = at.share;
            return {a.x * (1 - s) + b.x * sign * s, a.y * (1 - s) + b.y * sign * s, a.z * (1 - s) + b.z * sign * s,
                a.w * (1 - s) + b.w * sign * s};
        }
    }

    std::vector<Mat4> PoseSkeleton(std::span<const Joint> skeleton, std::span<const Motion* const> motions, float frame)
    {
        std::vector<const JointTrack*> tracks(skeleton.size(), nullptr);
        std::vector<Between> at(skeleton.size());
        for (const Motion* motion : motions)
            for (const JointTrack& track : motion->tracks)
                if (track.joint >= 0 && static_cast<size_t>(track.joint) < skeleton.size())
                {
                    tracks[static_cast<size_t>(track.joint)] = &track;
                    at[static_cast<size_t>(track.joint)]     = FramesAround(*motion, frame);
                }
        std::vector<Mat4> world(skeleton.size());
        for (size_t i = 0; i < skeleton.size(); ++i)
        {
            const Joint& joint = skeleton[i];
            Quat turn          = joint.rotation;
            Vec3 move          = joint.translation, size{1, 1, 1};
            if (const JointTrack* track = tracks[i])
            {
                turn = Times(BlendTurn(*track, at[i]), turn);
                move = Plus(move, {Blend(*track, kMoveX, at[i]), Blend(*track, kMoveY, at[i]), Blend(*track, kMoveZ, at[i])});
                // The root keeps its size, as Xim plays the game's animations.
                if (joint.parent >= 0) size = {Blend(*track, kSizeX, at[i]), Blend(*track, kSizeY, at[i]), Blend(*track, kSizeZ, at[i])};
            }
            const Mat4 local = Local(turn, move, size);
            world[i]         = joint.parent < 0 ? local : Times(local, world[static_cast<size_t>(joint.parent)]);
        }
        return world;
    }

    float MotionFrame(const Motion& motion, double seconds)
    {
        if (motion.frames <= 0) return 0.0f;
        return static_cast<float>(std::fmod(seconds * kTicksPerSecond * motion.speed, static_cast<double>(motion.frames)));
    }

    std::vector<DrawBatch> PosedBatches(std::span<const ModelMesh> meshes, std::span<const Mat4> joints)
    {
        std::vector<DrawBatch> batches;
        const auto batchOf = [&](const std::string& picture) -> DrawBatch& {
            for (DrawBatch& batch : batches)
                if (batch.picture == picture) return batch;
            return batches.emplace_back(DrawBatch{picture, {}});
        };
        for (const ModelMesh& mesh : meshes)
            for (int pass = 0; pass < (mesh.mirrored ? 2 : 1); ++pass)
            {
                const bool mirror = pass == 1;
                std::vector<DrawVertex> placed(mesh.vertices.size());
                std::vector<bool> held(mesh.vertices.size(), true);
                for (size_t i = 0; i < mesh.vertices.size(); ++i)
                {
                    const ModelVertex& v = mesh.vertices[i];
                    const auto& by       = mirror ? v.mirrorJoints : v.joints;
                    const int used       = v.twoJoints ? 2 : 1;
                    for (int k = 0; k < used; ++k)
                        held[i] = held[i] && by[static_cast<size_t>(k)] < joints.size();
                    if (!held[i]) continue;
                    const auto axis = [&](int k) { return mirror ? v.mirrorAxes[static_cast<size_t>(k)] : MirrorAxis::None; };
                    Vec3 at = Place(TurnedRound(v.positions[0], axis(0)), joints[by[0]], v.weight);
                    if (v.twoJoints) at = Plus(at, Place(TurnedRound(v.positions[1], axis(1)), joints[by[1]], 1.0f - v.weight));
                    const Vec3 normal = Unit(Place(TurnedRound(v.normal, axis(0)), joints[by[0]], 0.0f));
                    placed[i]         = {at.x, at.y, at.z, normal.x, normal.y, normal.z};
                }
                for (const ModelPart& part : mesh.parts)
                {
                    DrawBatch& batch = batchOf(part.textured ? part.picture : std::string());
                    for (size_t t = 0; t < part.triangles.size(); ++t)
                    {
                        const ModelTriangle& triangle = part.triangles[t];
                        const auto& v                 = triangle.vertices;
                        if (!held[v[0]] || !held[v[1]] || !held[v[2]]) continue;
                        const std::array<size_t, 3> corners = mirror ? std::array<size_t, 3>{0, 2, 1} : std::array<size_t, 3>{0, 1, 2};
                        for (size_t c : corners)
                        {
                            DrawVertex out = placed[v[c]];
                            out.u          = triangle.uv[c * 2];
                            out.v          = triangle.uv[c * 2 + 1];
                            if (!part.textured && t < part.colors.size()) out.color = ShownColor(part.colors[t]);
                            batch.vertices.push_back(out);
                        }
                    }
                }
            }
        return batches;
    }

    ModelBounds BoundsOf(std::span<const DrawBatch> batches)
    {
        ModelBounds box{{1e30f, 1e30f, 1e30f}, {-1e30f, -1e30f, -1e30f}};
        bool any = false;
        for (const DrawBatch& batch : batches)
            for (const DrawVertex& v : batch.vertices)
            {
                box.low  = {std::min(box.low.x, v.x), std::min(box.low.y, v.y), std::min(box.low.z, v.z)};
                box.high = {std::max(box.high.x, v.x), std::max(box.high.y, v.y), std::max(box.high.z, v.z)};
                any      = true;
            }
        return any ? box : ModelBounds{};
    }

    ModelView FramedView(const ModelBounds& box, float yaw, float aspect)
    {
        const Vec3 middle{(box.low.x + box.high.x) / 2, (box.low.y + box.high.y) / 2, (box.low.z + box.high.z) / 2};
        const Vec3 half{(box.high.x - box.low.x) / 2, (box.high.y - box.low.y) / 2, (box.high.z - box.low.z) / 2};
        const float radius   = std::max(std::sqrt(half.x * half.x + half.y * half.y + half.z * half.z), kNearest);
        const float across   = 2.0f * std::atan(std::tan(kFieldOfView / 2) * aspect);
        const float distance = radius / std::sin(std::min(kFieldOfView, across) / 2);
        const Vec3 eye{middle.x + std::sin(yaw) * distance, middle.y, middle.z - std::cos(yaw) * distance};
        const Vec3 up{0, -1, 0};

        const auto cross = [](const Vec3& a, const Vec3& b) { return Vec3{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; };
        const auto dot   = [](const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; };
        const Vec3 forward = Unit({middle.x - eye.x, middle.y - eye.y, middle.z - eye.z});
        const Vec3 right   = Unit(cross(up, forward));
        const Vec3 above   = cross(forward, right);
        ModelView out{};
        Mat4& v = out.view;
        v.m[kM11] = right.x, v.m[kM12] = above.x, v.m[kM13] = forward.x;
        v.m[kM21] = right.y, v.m[kM22] = above.y, v.m[kM23] = forward.y;
        v.m[kM31] = right.z, v.m[kM32] = above.z, v.m[kM33] = forward.z;
        v.m[kM41] = -dot(right, eye), v.m[kM42] = -dot(above, eye), v.m[kM43] = -dot(forward, eye), v.m[kM44] = 1;

        const float nearPlane = std::max(distance - radius * kDepthRoom, kNearest), farPlane = distance + radius * kDepthRoom;
        const float tall      = 1.0f / std::tan(kFieldOfView / 2);
        Mat4& p   = out.projection;
        p.m[kM11] = tall / aspect;
        p.m[kM22] = tall;
        p.m[kM33] = farPlane / (farPlane - nearPlane);
        p.m[kM34] = 1;
        p.m[kM43] = -nearPlane * farPlane / (farPlane - nearPlane);
        return out;
    }

    void Move(std::vector<DrawBatch>& batches, const Vec3& by)
    {
        for (DrawBatch& batch : batches)
            for (DrawVertex& v : batch.vertices)
            {
                v.x += by.x;
                v.y += by.y;
                v.z += by.z;
            }
    }

    ModelBounds Moved(const ModelBounds& box, const Vec3& by) { return {Plus(box.low, by), Plus(box.high, by)}; }

    ModelBounds Joined(const ModelBounds& a, const ModelBounds& b)
    {
        return {{std::min(a.low.x, b.low.x), std::min(a.low.y, b.low.y), std::min(a.low.z, b.low.z)},
            {std::max(a.high.x, b.high.x), std::max(a.high.y, b.high.y), std::max(a.high.z, b.high.z)}};
    }

    Vec3 HeadOf(const ModelBounds& box) { return {(box.low.x + box.high.x) / 2, box.low.y, (box.low.z + box.high.z) / 2}; }

    std::optional<PicturePoint> PointInPicture(const ModelView& view, const Vec3& point, float width, float height)
    {
        const std::array<float, kSide> clip = RowTimes(RowTimes({point.x, point.y, point.z, 1.0f}, view.view), view.projection);
        if (clip[3] <= 0.0f) return std::nullopt;
        return PicturePoint{(clip[0] / clip[3] + 1.0f) / 2.0f * width, (1.0f - clip[1] / clip[3]) / 2.0f * height};
    }
}
