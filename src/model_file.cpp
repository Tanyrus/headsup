#include "model_file.h"

#include "bytes.h"

#include <algorithm>
#include <cstdio>
#include <iterator>
#include <utility>

namespace headsup
{
    namespace
    {
        constexpr size_t kTableEntryBytes = 2;
        constexpr int kEntryFolderShift   = 7;
        constexpr uint16_t kEntryFileMask = 0x7F;

        std::string RomFolder(int folder) { return folder == 1 ? std::string("ROM") : "ROM" + std::to_string(folder); }

        // Where a monster's model number lands: each range of numbers has its own run of files.
        struct NpcRange
        {
            uint16_t from;
            uint32_t offset;
        };
        constexpr NpcRange kNpcRanges[]{{3193, 98546}, {3000, 96907}, {1500, 50295}, {0, 1300}};

        // The DLL's tables open with these bytes in this client, and lie past its code.
        constexpr uint8_t kPiecesStart[]{0xA8, 0x1B, 0x00, 0x00};
        constexpr uint8_t kSkeletonsStart[]{0xA0, 0x1B, 0xA0, 0x1B};
        constexpr size_t kTablesFrom = 0x30000, kTableAlign = 4;
        constexpr size_t kRunBytes = 8, kSlotBytes = PlayerTables::kRuns * kRunBytes, kRaceBytes = kLookSlots * kSlotBytes;
        constexpr size_t kSkeletonBytes = 2;
        constexpr uint32_t kUpperBodyMotionFile = 1; // after the skeleton's file
        constexpr uint16_t kLookModelMask      = 0x0FFF;
        constexpr uint16_t kFaceMask           = 0xFF; // a look's first word: the face, then the race
        constexpr int kRaceShift               = 8;

        std::optional<size_t> FindTable(std::span<const uint8_t> dll, std::span<const uint8_t> start, size_t bytes)
        {
            for (size_t at = kTablesFrom; at + bytes <= dll.size(); at += kTableAlign)
                if (std::equal(start.begin(), start.end(), dll.begin() + static_cast<std::ptrdiff_t>(at))) return at;
            return std::nullopt;
        }

        // Reads a chunk's body front to back, and remembers when a read ran past its end.
        class Reader
        {
          public:
            Reader(std::span<const uint8_t> body, size_t at) : m_Body(body), m_At(at), m_Ok(at <= body.size()) {}
            bool Has(size_t bytes) const { return m_Ok && bytes <= m_Body.size() - m_At; }
            template <typename T>
            T Take()
            {
                if (!Has(sizeof(T)))
                {
                    m_Ok = false;
                    return T{};
                }
                const T value = ReadAt<T>(m_Body.data(), m_At);
                m_At += sizeof(T);
                return value;
            }
            float Float() { return Take<float>(); }
            std::string Text(size_t bytes)
            {
                if (!Has(bytes))
                {
                    m_Ok = false;
                    return {};
                }
                std::string text(reinterpret_cast<const char*>(m_Body.data() + m_At), bytes);
                m_At += bytes;
                return text;
            }
            void Skip(size_t bytes)
            {
                if (Has(bytes))
                    m_At += bytes;
                else
                    m_Ok = false;
            }
            bool Ok() const { return m_Ok; }

          private:
            std::span<const uint8_t> m_Body;
            size_t m_At;
            bool m_Ok;
        };

        // A skeleton: two bytes, the joint count, a byte, then each joint's parent (itself for the root), a byte,
        // its turn as x, y, z, w and its place.
        constexpr size_t kJointCountAt = 2, kJointsAt = 4, kJointPad = 1;

        Read<std::vector<Joint>> ReadSkeleton(std::span<const uint8_t> body)
        {
            if (body.size() <= kJointCountAt) return {std::nullopt, "its skeleton ends early"};
            const size_t count = body[kJointCountAt];
            Reader in(body, kJointsAt);
            std::vector<Joint> joints(count);
            for (size_t i = 0; i < count; ++i)
            {
                const size_t parent = in.Take<uint8_t>();
                in.Skip(kJointPad);
                if (parent > i) return {std::nullopt, "its skeleton's joint " + std::to_string(i) + " comes before its parent"};
                joints[i].parent      = parent == i ? -1 : static_cast<int>(parent);
                joints[i].rotation    = {in.Float(), in.Float(), in.Float(), in.Float()};
                joints[i].translation = {in.Float(), in.Float(), in.Float()};
            }
            if (!in.Ok()) return {std::nullopt, "its skeleton ends early"};
            return {std::move(joints), {}};
        }

        // A mesh's header gives where its parts are in 16-bit words from its start: its draw commands, its list of
        // joints (with their count), its vertex counts, its joint references and its vertices.
        constexpr size_t kMeshFlagsAt = 2, kMirroredAt = 4, kCommandsAt = 6, kJointListAt = 12, kJointListCountAt = 16,
                         kVertexCountsAt = 18, kRefsAt = 24, kVerticesAt = 30, kMeshHeaderBytes = 34;
        constexpr uint8_t kClothFlag = 0x01, kJointListFlag = 0x80;
        constexpr size_t kWordBytes  = 2;
        // A joint reference: the joint, the joint that draws it mirrored and the axis turned round for that.
        constexpr uint16_t kRefJointMask = 0x7F, kRefAxisMask = 3;
        constexpr int kRefMirrorShift = 7, kRefAxisShift = 14;

        enum Command : uint16_t
        {
            kEnd         = 0xFFFF,
            kMaterial    = 0x8010,
            kPicture     = 0x8000,
            kList        = 0x0054,
            kStrip       = 0x5453,
            kColorList   = 0x0043,
            kColorStrip  = 0x4353,
        };
        constexpr size_t kMaterialBytes = 44, kPictureNameBytes = 16;

        size_t WordsAt(std::span<const uint8_t> body, size_t at) { return ReadAt<uint32_t>(body.data(), at) * kWordBytes; }

        std::string MeshFailure(const std::string& name, const std::string& what) { return "its mesh " + name + " " + what; }

        Read<ModelMesh> ReadMesh(std::string name, std::span<const uint8_t> body)
        {
            if (body.size() < kMeshHeaderBytes) return {std::nullopt, MeshFailure(name, "ends early")};
            ModelMesh mesh;
            mesh.name            = std::move(name);
            const uint8_t flags  = body[kMeshFlagsAt];
            const bool cloth     = (flags & kClothFlag) != 0;
            const bool listed    = (flags & kJointListFlag) != 0;
            mesh.mirrored        = body[kMirroredAt] == 1;

            Reader list(body, WordsAt(body, kJointListAt));
            std::vector<uint16_t> jointList(ReadAt<uint16_t>(body.data(), kJointListCountAt));
            for (uint16_t& joint : jointList)
                joint = list.Take<uint16_t>();
            Reader counts(body, WordsAt(body, kVertexCountsAt));
            const size_t oneJoint = counts.Take<uint16_t>(), twoJoints = counts.Take<uint16_t>();
            if (!list.Ok() || !counts.Ok()) return {std::nullopt, MeshFailure(mesh.name, "ends early")};

            bool badJoint   = false;
            const auto joint = [&](uint16_t index) -> uint8_t {
                if (!listed) return static_cast<uint8_t>(index);
                if (index >= jointList.size())
                {
                    badJoint = true;
                    return 0;
                }
                return static_cast<uint8_t>(jointList[index]);
            };
            mesh.vertices.resize(oneJoint + twoJoints);
            Reader refs(body, WordsAt(body, kRefsAt));
            for (ModelVertex& v : mesh.vertices)
                for (size_t k = 0; k < 2; ++k)
                {
                    const auto ref     = refs.Take<uint16_t>();
                    v.joints[k]        = joint(ref & kRefJointMask);
                    v.mirrorJoints[k]  = joint(ref >> kRefMirrorShift & kRefJointMask);
                    v.mirrorAxes[k]    = static_cast<uint8_t>(ref >> kRefAxisShift & kRefAxisMask);
                }
            Reader in(body, WordsAt(body, kVerticesAt));
            for (size_t i = 0; i < mesh.vertices.size(); ++i)
            {
                ModelVertex& v = mesh.vertices[i];
                if (i < oneJoint)
                {
                    v.positions[0] = {in.Float(), in.Float(), in.Float()};
                    if (!cloth) v.normal = {in.Float(), in.Float(), in.Float()};
                    continue;
                }
                v.twoJoints = true;
                float p[6];
                for (float& f : p)
                    f = in.Float();
                v.positions = {Vec3{p[0], p[2], p[4]}, Vec3{p[1], p[3], p[5]}};
                v.weight    = in.Float();
                in.Float();
                if (!cloth)
                {
                    float n[6];
                    for (float& f : n)
                        f = in.Float();
                    v.normal = {n[0], n[2], n[4]};
                }
            }
            if (!refs.Ok() || !in.Ok()) return {std::nullopt, MeshFailure(mesh.name, "ends early")};
            if (badJoint) return {std::nullopt, MeshFailure(mesh.name, "names a joint past its list")};

            const auto vertex = [&](Reader& r, bool& bad) {
                const auto index = r.Take<uint16_t>();
                bad              = bad || index >= mesh.vertices.size();
                return index;
            };
            Reader commands(body, WordsAt(body, kCommandsAt));
            std::string picture;
            bool badVertex = false;
            for (;;)
            {
                const auto command = commands.Take<uint16_t>();
                if (!commands.Ok()) return {std::nullopt, MeshFailure(mesh.name, "ends early")};
                if (command == kEnd) break;
                ModelPart part;
                part.picture = picture;
                switch (command)
                {
                case kMaterial: commands.Skip(kMaterialBytes); continue;
                case kPicture: picture = commands.Text(kPictureNameBytes); continue;
                case kList:
                {
                    const auto n = commands.Take<uint16_t>();
                    for (uint16_t t = 0; t < n && commands.Ok(); ++t)
                    {
                        ModelTriangle triangle;
                        for (uint16_t& v : triangle.vertices)
                            v = vertex(commands, badVertex);
                        for (float& f : triangle.uv)
                            f = commands.Float();
                        part.triangles.push_back(triangle);
                    }
                    break;
                }
                case kStrip:
                {
                    // Each corner after the first three makes a triangle with the two before it; every second one is
                    // wound back to face the same way, and those with a corner twice have no area.
                    const auto n = commands.Take<uint16_t>();
                    std::vector<std::pair<uint16_t, std::array<float, 2>>> corners(3);
                    for (auto& c : corners)
                        c.first = vertex(commands, badVertex);
                    for (auto& c : corners)
                        c.second = {commands.Float(), commands.Float()};
                    for (uint16_t k = 1; k < n && commands.Ok(); ++k)
                    {
                        const uint16_t index = vertex(commands, badVertex);
                        corners.push_back({index, {commands.Float(), commands.Float()}});
                    }
                    for (size_t k = 0; k + 2 < corners.size(); ++k)
                    {
                        const std::array<size_t, 3> at = k % 2 == 0 ? std::array<size_t, 3>{k, k + 1, k + 2} : std::array<size_t, 3>{k, k + 2, k + 1};
                        ModelTriangle triangle;
                        for (size_t c = 0; c < 3; ++c)
                        {
                            triangle.vertices[c]  = corners[at[c]].first;
                            triangle.uv[c * 2]     = corners[at[c]].second[0];
                            triangle.uv[c * 2 + 1] = corners[at[c]].second[1];
                        }
                        const auto& v = triangle.vertices;
                        if (v[0] != v[1] && v[1] != v[2] && v[0] != v[2]) part.triangles.push_back(triangle);
                    }
                    break;
                }
                case kColorList:
                case kColorStrip:
                {
                    part.picture.clear();
                    part.textured = false;
                    const auto n  = commands.Take<uint16_t>();
                    std::vector<uint16_t> corners;
                    for (uint16_t t = 0; t < (command == kColorList ? n : 1) && commands.Ok(); ++t)
                    {
                        ModelTriangle triangle;
                        for (uint16_t& v : triangle.vertices)
                            v = vertex(commands, badVertex);
                        part.colors.push_back(commands.Take<uint32_t>());
                        part.triangles.push_back(triangle);
                        corners.assign(triangle.vertices.begin(), triangle.vertices.end());
                    }
                    if (command == kColorStrip)
                        for (uint16_t k = 1; k < n && commands.Ok(); ++k)
                        {
                            corners.push_back(vertex(commands, badVertex));
                            const size_t last = corners.size() - 1;
                            part.triangles.push_back({{corners[last - 2], corners[last - 1], corners[last]}, {}});
                            part.colors.push_back(part.colors.front());
                        }
                    break;
                }
                default:
                {
                    char code[8];
                    std::snprintf(code, sizeof(code), "0x%04X", command);
                    return {std::nullopt, MeshFailure(mesh.name, std::string("has a draw command ") + code + " HeadsUp does not know")};
                }
                }
                if (!commands.Ok()) return {std::nullopt, MeshFailure(mesh.name, "ends early")};
                mesh.parts.push_back(std::move(part));
            }
            if (badVertex) return {std::nullopt, MeshFailure(mesh.name, "draws a vertex it does not have")};
            return {std::move(mesh), {}};
        }

        // A motion: a word, its joint count, its frame count and speed, then from kPoolAt the numbers it counts its
        // frames' places in: first each joint's record (its joint, then for its turn, move and size where each
        // channel's frames start, and the number each has when they do not change), then the frames.
        constexpr size_t kMotionJointsAt = 2, kMotionFramesAt = 4, kMotionSpeedAt = 6, kPoolAt = 10, kRecordBytes = 84;
        constexpr int kChannelGroups[]{4, 3, 3}; // turn, move, size

        Read<Motion> ReadMotion(std::string name, std::span<const uint8_t> body)
        {
            if (body.size() < kPoolAt) return {std::nullopt, "its motion " + name + " ends early"};
            Motion motion;
            motion.name             = std::move(name);
            const size_t joints     = ReadAt<uint16_t>(body.data(), kMotionJointsAt);
            motion.frames           = ReadAt<uint16_t>(body.data(), kMotionFramesAt);
            motion.speed            = ReadAt<float>(body.data(), kMotionSpeedAt);
            const auto pool         = body.subspan(kPoolAt);
            for (size_t j = 0; j < joints; ++j)
            {
                Reader in(pool, j * kRecordBytes);
                JointTrack track;
                track.joint = in.Take<int32_t>();
                std::array<int32_t, JointTrack::kChannels> at{};
                bool skipped = false;
                int channel  = 0;
                for (int count : kChannelGroups)
                {
                    for (int c = 0; c < count; ++c)
                    {
                        at[static_cast<size_t>(channel + c)] = in.Take<int32_t>();
                        skipped = skipped || at[static_cast<size_t>(channel + c)] < 0;
                    }
                    for (int c = 0; c < count; ++c)
                        track.channels[static_cast<size_t>(channel + c)] = {in.Float()};
                    channel += count;
                }
                if (!in.Ok()) return {std::nullopt, "its motion " + motion.name + " ends early"};
                if (skipped) continue;
                for (size_t c = 0; c < JointTrack::kChannels; ++c)
                {
                    if (at[c] == 0) continue;
                    Reader frames(pool, static_cast<size_t>(at[c]) * sizeof(float));
                    std::vector<float>& values = track.channels[c];
                    values.resize(static_cast<size_t>(motion.frames));
                    for (float& f : values)
                        f = frames.Float();
                    if (!frames.Ok()) return {std::nullopt, "its motion " + motion.name + "'s frames run past its end"};
                }
                motion.tracks.push_back(std::move(track));
            }
            return {std::move(motion), {}};
        }
    }

    std::string VTableFile(int folder) { return folder == 1 ? "VTABLE.DAT" : RomFolder(folder) + "\\VTABLE" + std::to_string(folder) + ".DAT"; }
    std::string FTableFile(int folder) { return folder == 1 ? "FTABLE.DAT" : RomFolder(folder) + "\\FTABLE" + std::to_string(folder) + ".DAT"; }

    std::optional<std::string> DatFileOf(const FileTables& tables, uint32_t fileId)
    {
        for (int folder = 1; folder <= kRomFolders; ++folder)
        {
            const RomTables& pair = tables[static_cast<size_t>(folder - 1)];
            const size_t entryAt  = static_cast<size_t>(fileId) * kTableEntryBytes;
            if (fileId >= pair.vtable.size() || pair.vtable[fileId] != folder || entryAt + kTableEntryBytes > pair.ftable.size())
                continue;
            const auto entry = ReadAt<uint16_t>(pair.ftable.data(), entryAt);
            return RomFolder(folder) + "\\" + std::to_string(entry >> kEntryFolderShift) + "\\" + std::to_string(entry & kEntryFileMask) + ".DAT";
        }
        return std::nullopt;
    }

    uint32_t NpcModelFileId(uint16_t model)
    {
        for (const NpcRange& range : kNpcRanges)
            if (model >= range.from) return range.offset + model;
        return model;
    }

    std::optional<PlayerTables> FindPlayerTables(std::span<const uint8_t> dll)
    {
        const auto pieces    = FindTable(dll, kPiecesStart, kRaceBytes * kPlayerRaces);
        const auto skeletons = FindTable(dll, kSkeletonsStart, kSkeletonBytes * (kPlayerRaces + 1));
        if (!pieces || !skeletons) return std::nullopt;
        PlayerTables tables;
        for (size_t race = 0; race < kPlayerRaces; ++race)
        {
            for (size_t slot = 0; slot < kLookSlots; ++slot)
                for (size_t run = 0; run < PlayerTables::kRuns; ++run)
                {
                    const size_t at = *pieces + race * kRaceBytes + slot * kSlotBytes + run * kRunBytes;
                    tables.pieces[race][slot][run] = {ReadAt<uint32_t>(dll.data(), at), ReadAt<uint32_t>(dll.data(), at + 4)};
                }
            // The skeletons are kept by race number from 0, so race 1's is the second.
            tables.skeletons[race] = ReadAt<uint16_t>(dll.data(), *skeletons + (race + 1) * kSkeletonBytes);
        }
        return tables;
    }

    std::optional<uint32_t> PieceFileId(const PlayerTables& tables, int race, LookSlot slot, uint16_t model)
    {
        if (race < 1 || race > kPlayerRaces) return std::nullopt;
        uint32_t counted = 0;
        for (const PlayerTables::Run& run : tables.pieces[static_cast<size_t>(race - 1)][static_cast<size_t>(slot)])
        {
            if (run.first == 0) continue;
            if (model < counted + run.count) return run.first + (model - counted);
            counted += run.count;
        }
        return std::nullopt;
    }

    std::optional<uint32_t> SkeletonFileId(const PlayerTables& tables, int race)
    {
        if (race < 1 || race > kPlayerRaces) return std::nullopt;
        return tables.skeletons[static_cast<size_t>(race - 1)];
    }

    std::optional<std::vector<uint32_t>> PlayerFileIds(const PlayerTables& tables, const PlayerLook& look)
    {
        const int race      = look[0] >> kRaceShift;
        const auto skeleton = SkeletonFileId(tables, race);
        const auto face     = PieceFileId(tables, race, LookSlot::Face, static_cast<uint16_t>(look[0] & kFaceMask));
        if (!skeleton || !face) return std::nullopt;
        std::vector<uint32_t> files{*skeleton, *skeleton + kUpperBodyMotionFile, *face};
        for (int slot = static_cast<int>(LookSlot::Head); slot < kLookSlots; ++slot)
        {
            const auto model = static_cast<uint16_t>(look[static_cast<size_t>(slot)] & kLookModelMask);
            if (model == 0 && slot >= static_cast<int>(LookSlot::Main)) continue;
            const auto file = PieceFileId(tables, race, static_cast<LookSlot>(slot), model);
            if (!file) return std::nullopt;
            files.push_back(*file);
        }
        return files;
    }

    float JointTrack::At(int channel, int frame) const
    {
        const std::vector<float>& values = channels[static_cast<size_t>(channel)];
        if (values.empty()) return 0.0f;
        return values.size() == 1 ? values[0] : values[std::clamp<size_t>(static_cast<size_t>(std::max(frame, 0)), 0, values.size() - 1)];
    }

    Read<ModelFile> ReadModelFile(std::span<const uint8_t> dat)
    {
        const auto chunks = DatChunks(dat);
        if (!chunks) return {std::nullopt, "its chunks run past its end"};
        ModelFile model;
        for (const DatChunk& chunk : *chunks)
        {
            std::string name(chunk.name);
            switch (chunk.type)
            {
            case kPictureChunkType:
            {
                Read<ColorPicture> picture = ReadColorPicture(chunk.body);
                if (!picture.value) return {std::nullopt, std::move(picture.failure)};
                model.pictures.push_back(std::move(*picture.value));
                break;
            }
            case kSkeletonChunkType:
            {
                if (!model.skeleton.empty()) break;
                Read<std::vector<Joint>> skeleton = ReadSkeleton(chunk.body);
                if (!skeleton.value) return {std::nullopt, std::move(skeleton.failure)};
                model.skeleton = std::move(*skeleton.value);
                break;
            }
            case kMeshChunkType:
            {
                Read<ModelMesh> mesh = ReadMesh(std::move(name), chunk.body);
                if (!mesh.value) return {std::nullopt, std::move(mesh.failure)};
                model.meshes.push_back(std::move(*mesh.value));
                break;
            }
            case kMotionChunkType:
            {
                Read<Motion> motion = ReadMotion(std::move(name), chunk.body);
                if (!motion.value) return {std::nullopt, std::move(motion.failure)};
                model.motions.push_back(std::move(*motion.value));
                break;
            }
            default: break;
            }
        }
        return {std::move(model), {}};
    }

    void AddModelFile(ModelFile& model, ModelFile part)
    {
        if (model.skeleton.empty()) model.skeleton = std::move(part.skeleton);
        const auto take = [](auto& into, auto& from) { into.insert(into.end(), std::make_move_iterator(from.begin()), std::make_move_iterator(from.end())); };
        take(model.meshes, part.meshes);
        take(model.pictures, part.pictures);
        take(model.motions, part.motions);
    }

    std::vector<const Motion*> IdleMotions(const ModelFile& model)
    {
        std::vector<const Motion*> idle;
        for (const Motion& motion : model.motions)
            if (std::find(std::begin(kIdleMotions), std::end(kIdleMotions), motion.name) != std::end(kIdleMotions)) idle.push_back(&motion);
        return idle;
    }

    std::vector<uint8_t> ShownPixels(const ColorPicture& picture)
    {
        constexpr size_t kAlphaAt = 3, kPixelBytes = 4;
        constexpr int kOpaque     = 0x80, kFull = 255;
        std::vector<uint8_t> shown = picture.bgra;
        for (size_t at = kAlphaAt; at < shown.size(); at += kPixelBytes)
            shown[at] = static_cast<uint8_t>(std::min(kFull, shown[at] * kFull / kOpaque));
        return shown;
    }
}
