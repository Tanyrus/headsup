#include "dat_bytes.h"
#include "model_file.h"
#include "model_pose.h"
#include "test.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

using namespace headsup;

namespace
{
    using namespace dat;

    void PutFloat(Bytes& out, float v)
    {
        uint32_t bits = 0;
        std::memcpy(&bits, &v, sizeof(bits));
        Put32(out, bits);
    }
    bool Near(float a, float b) { return std::fabs(a - b) < 1e-5f; }

    // FTABLE's entry for a file: the folder under the ROM folder in the high 9 bits, the file in the low 7.
    uint16_t Entry(int folder, int file) { return static_cast<uint16_t>(folder << 7 | file); }

    FileTables Tables(int ids)
    {
        FileTables tables;
        for (RomTables& pair : tables)
        {
            pair.vtable.assign(static_cast<size_t>(ids), 0);
            pair.ftable.assign(static_cast<size_t>(ids) * 2, 0);
        }
        return tables;
    }
    void Own(FileTables& tables, int folder, uint32_t id, uint16_t entry)
    {
        RomTables& pair                 = tables[static_cast<size_t>(folder - 1)];
        pair.vtable[id]                 = static_cast<uint8_t>(folder);
        pair.ftable[id * 2]             = static_cast<uint8_t>(entry);
        pair.ftable[id * 2 + 1]         = static_cast<uint8_t>(entry >> 8);
    }

    // A skeleton chunk's body: two bytes, the joint count, a byte, then each joint as its parent (itself for the
    // root), a byte, its turn as x, y, z, w and its place.
    Bytes SkeletonBody(const std::vector<Joint>& joints)
    {
        Bytes out{0, 0, static_cast<uint8_t>(joints.size()), 0};
        for (size_t i = 0; i < joints.size(); ++i)
        {
            out.push_back(static_cast<uint8_t>(joints[i].parent < 0 ? i : static_cast<size_t>(joints[i].parent)));
            out.push_back(0);
            for (float v : {joints[i].rotation.x, joints[i].rotation.y, joints[i].rotation.z, joints[i].rotation.w})
                PutFloat(out, v);
            for (float v : {joints[i].translation.x, joints[i].translation.y, joints[i].translation.z})
                PutFloat(out, v);
        }
        return out;
    }

    // A vertex's two joint references: a joint, its mirror and the mirror's axis, packed as the game packs them.
    uint16_t Ref(int joint, int mirror, int axis) { return static_cast<uint16_t>(joint | mirror << 7 | axis << 14); }

    // A mesh chunk's body: a header of offsets counted in 16-bit words, then the joint list, the vertex counts, the
    // joint references (two a vertex), the vertices and the draw commands.
    Bytes MeshBody(uint8_t flags, bool mirrored, const std::vector<uint16_t>& jointList, uint16_t oneJoint, uint16_t twoJoints,
        const std::vector<std::array<uint16_t, 2>>& refs, const std::vector<float>& vertices, const Bytes& commands)
    {
        constexpr size_t kHeader = 36;
        const size_t listAt = kHeader, countsAt = listAt + jointList.size() * 2, refsAt = countsAt + 4,
                     verticesAt = refsAt + refs.size() * 4, commandsAt = verticesAt + vertices.size() * 4;
        Bytes out(kHeader, 0);
        out[2] = flags;
        out[4] = mirrored ? 1 : 0;
        auto put32At = [&](size_t at, uint32_t v) {
            for (int i = 0; i < 4; ++i)
                out[at + static_cast<size_t>(i)] = static_cast<uint8_t>(v >> (8 * i));
        };
        put32At(6, static_cast<uint32_t>(commandsAt / 2));
        put32At(12, static_cast<uint32_t>(listAt / 2));
        out[16] = static_cast<uint8_t>(jointList.size());
        put32At(18, static_cast<uint32_t>(countsAt / 2));
        put32At(24, static_cast<uint32_t>(refsAt / 2));
        put32At(30, static_cast<uint32_t>(verticesAt / 2));
        for (uint16_t j : jointList)
            Put16(out, j);
        Put16(out, oneJoint);
        Put16(out, twoJoints);
        for (const auto& pair : refs)
        {
            Put16(out, pair[0]);
            Put16(out, pair[1]);
        }
        for (float v : vertices)
            PutFloat(out, v);
        return out + commands;
    }

    Bytes TextureCommand(const char* name)
    {
        Bytes out;
        Put16(out, 0x8000);
        Bytes padded(name, name + std::strlen(name));
        padded.resize(16, ' ');
        return out + padded;
    }
    struct Corner
    {
        uint16_t vertex;
        float u, v;
    };
    Bytes ListCommand(const std::vector<std::array<Corner, 3>>& triangles)
    {
        Bytes out;
        Put16(out, 0x0054);
        Put16(out, static_cast<uint16_t>(triangles.size()));
        for (const auto& t : triangles)
        {
            for (const Corner& c : t)
                Put16(out, c.vertex);
            for (const Corner& c : t)
            {
                PutFloat(out, c.u);
                PutFloat(out, c.v);
            }
        }
        return out;
    }
    Bytes StripCommand(const std::vector<Corner>& corners)
    {
        Bytes out;
        Put16(out, 0x5453);
        Put16(out, static_cast<uint16_t>(corners.size() - 2));
        for (size_t i = 0; i < 3; ++i)
            Put16(out, corners[i].vertex);
        for (size_t i = 0; i < 3; ++i)
        {
            PutFloat(out, corners[i].u);
            PutFloat(out, corners[i].v);
        }
        for (size_t i = 3; i < corners.size(); ++i)
        {
            Put16(out, corners[i].vertex);
            PutFloat(out, corners[i].u);
            PutFloat(out, corners[i].v);
        }
        return out;
    }
    Bytes MaterialCommand()
    {
        Bytes out;
        Put16(out, 0x8010);
        out.resize(out.size() + 44, 0x5A);
        return out;
    }
    Bytes ColorListCommand(const std::vector<std::pair<std::array<uint16_t, 3>, uint32_t>>& triangles)
    {
        Bytes out;
        Put16(out, 0x0043);
        Put16(out, static_cast<uint16_t>(triangles.size()));
        for (const auto& [corners, color] : triangles)
        {
            for (uint16_t c : corners)
                Put16(out, c);
            Put32(out, color);
        }
        return out;
    }
    // A strip in one color: its first triangle and color, then each corner after.
    Bytes ColorStripCommand(const std::vector<uint16_t>& corners, uint32_t color)
    {
        Bytes out;
        Put16(out, 0x4353);
        Put16(out, static_cast<uint16_t>(corners.size() - 2));
        for (size_t i = 0; i < 3; ++i)
            Put16(out, corners[i]);
        Put32(out, color);
        for (size_t i = 3; i < corners.size(); ++i)
            Put16(out, corners[i]);
        return out;
    }
    Bytes EndCommand()
    {
        Bytes out;
        Put16(out, 0xFFFF);
        return out;
    }

    // An animation chunk's body: a word, the joint count, the frame count, the speed, then each joint's record of
    // 21 numbers (its index, then for its turn, move and size where each channel's frames are in the numbers from
    // the first record on, 0 for the same throughout, and those constants), then the frames.
    struct TrackBytes
    {
        int32_t joint;
        std::array<int32_t, 10> at;
        std::array<float, 10> constants;
    };
    Bytes MotionBody(uint16_t frames, float speed, const std::vector<TrackBytes>& tracks, const std::vector<float>& pool)
    {
        Bytes out;
        Put16(out, 0);
        Put16(out, static_cast<uint16_t>(tracks.size()));
        Put16(out, frames);
        PutFloat(out, speed);
        for (const TrackBytes& t : tracks)
        {
            Put32(out, static_cast<uint32_t>(t.joint));
            for (int c = 0; c < 4; ++c)
                Put32(out, static_cast<uint32_t>(t.at[static_cast<size_t>(c)]));
            for (int c = 0; c < 4; ++c)
                PutFloat(out, t.constants[static_cast<size_t>(c)]);
            for (int part = 0; part < 2; ++part)
            {
                for (int c = 0; c < 3; ++c)
                    Put32(out, static_cast<uint32_t>(t.at[static_cast<size_t>(4 + part * 3 + c)]));
                for (int c = 0; c < 3; ++c)
                    PutFloat(out, t.constants[static_cast<size_t>(4 + part * 3 + c)]);
            }
        }
        for (float v : pool)
            PutFloat(out, v);
        return out;
    }
    constexpr int32_t kRecordFloats = 21;
    constexpr std::array<float, 10> kStill{0, 0, 0, 1, 0, 0, 0, 1, 1, 1};

    std::string GameFolder()
    {
        const char* home = std::getenv("HOME");
        return home ? std::string(home) + "/Games/PhoenixXI/SquareEnix/FINAL FANTASY XI/" : std::string();
    }
    Bytes FileBytes(const std::string& path)
    {
        std::ifstream in(path, std::ios::binary);
        return Bytes(std::istreambuf_iterator<char>(in), {});
    }
    // A file under the game's folder named as the game names it.
    Bytes GameFile(std::string file)
    {
        for (char& c : file)
            if (c == '\\') c = '/';
        return FileBytes(GameFolder() + file);
    }
}

TEST(a_file_id_is_found_in_whichever_rom_folder_owns_it)
{
    FileTables tables = Tables(10);
    Own(tables, 1, 5, Entry(3, 9));
    Own(tables, 2, 7, Entry(22, 48));
    Own(tables, 9, 8, Entry(511, 127));
    CHECK_EQ(DatFileOf(tables, 5).value_or(""), std::string("ROM\\3\\9.DAT"));
    CHECK_EQ(DatFileOf(tables, 7).value_or(""), std::string("ROM2\\22\\48.DAT"));
    CHECK_EQ(DatFileOf(tables, 8).value_or(""), std::string("ROM9\\511\\127.DAT"));
    CHECK(!DatFileOf(tables, 6));
    CHECK(!DatFileOf(tables, 10));
}

TEST(a_folder_owns_an_id_only_where_its_vtable_holds_its_own_number)
{
    FileTables tables = Tables(4);
    Own(tables, 3, 2, Entry(1, 1));
    tables[1].vtable[2] = 1; // ROM2's table naming another folder at the id
    CHECK_EQ(DatFileOf(tables, 2).value_or(""), std::string("ROM3\\1\\1.DAT"));
}

TEST(the_rom_folders_table_files_are_named_as_the_game_names_them)
{
    CHECK_EQ(VTableFile(1), std::string("VTABLE.DAT"));
    CHECK_EQ(FTableFile(1), std::string("FTABLE.DAT"));
    CHECK_EQ(VTableFile(4), std::string("ROM4\\VTABLE4.DAT"));
    CHECK_EQ(FTableFile(9), std::string("ROM9\\FTABLE9.DAT"));
}

TEST(monster_model_numbers_find_their_files_by_range)
{
    CHECK_EQ(NpcModelFileId(1202), 2502u); // the Tonberry Cutter, ROM2\22\48.DAT
    CHECK_EQ(NpcModelFileId(1499), 2799u);
    CHECK_EQ(NpcModelFileId(1500), 51795u);
    CHECK_EQ(NpcModelFileId(3000), 99907u);
    CHECK_EQ(NpcModelFileId(3193), 101739u);
}

namespace
{
    constexpr size_t kDllBytes = 0x40000, kPiecesAt = 0x34C78, kSkeletonsAt = 0x36810;
    constexpr size_t kRaceBytes = 0x1B0, kSlotBytes = 0x30;

    void Put32At(Bytes& out, size_t at, uint32_t v)
    {
        for (int i = 0; i < 4; ++i)
            out[at + static_cast<size_t>(i)] = static_cast<uint8_t>(v >> (8 * i));
    }
    // A DLL with the two tables where this client has them: Hume male's face runs from 0x1BA8 and both of the first
    // two races' skeletons are 0x1BA0, which are the values the tables are found by.
    Bytes Dll()
    {
        Bytes dll(kDllBytes, 0);
        Put32At(dll, kPiecesAt, 0x1BA8);
        Put32At(dll, kPiecesAt + 4, 3);
        dll[kSkeletonsAt] = 0xA0, dll[kSkeletonsAt + 1] = 0x1B, dll[kSkeletonsAt + 2] = 0xA0, dll[kSkeletonsAt + 3] = 0x1B;
        return dll;
    }
}

TEST(a_race_piece_counts_its_model_number_along_its_runs_of_files)
{
    Bytes dll                 = Dll();
    const size_t taruFemaleBody = kPiecesAt + kRaceBytes * 5 + kSlotBytes * 2;
    Put32At(dll, taruFemaleBody, 20078);
    Put32At(dll, taruFemaleBody + 4, 5);
    Put32At(dll, taruFemaleBody + 8, 30000);
    Put32At(dll, taruFemaleBody + 12, 10);
    dll[kSkeletonsAt + 12] = 0x40, dll[kSkeletonsAt + 13] = 0x4D; // race 6: 19776
    const auto tables = FindPlayerTables(dll);
    CHECK(tables.has_value());
    CHECK_EQ(PieceFileId(*tables, 6, LookSlot::Body, 3).value_or(0), 20081u);
    CHECK_EQ(PieceFileId(*tables, 6, LookSlot::Body, 7).value_or(0), 30002u);
    CHECK(!PieceFileId(*tables, 6, LookSlot::Body, 15));
    CHECK_EQ(PieceFileId(*tables, 1, LookSlot::Face, 2).value_or(0), 0x1BAAu);
    CHECK_EQ(SkeletonFileId(*tables, 6).value_or(0), 19776u);
    CHECK_EQ(SkeletonFileId(*tables, 1).value_or(0), 0x1BA0u);
    CHECK(!SkeletonFileId(*tables, 0));
    CHECK(!PieceFileId(*tables, 9, LookSlot::Body, 0));
}

TEST(a_player_is_drawn_from_their_races_skeleton_and_motions_then_their_face_and_pieces)
{
    Bytes dll = Dll();
    const size_t taru = kPiecesAt + kRaceBytes * 5;
    for (size_t slot = 0; slot < kLookSlots; ++slot)
    {
        Put32At(dll, taru + kSlotBytes * slot, static_cast<uint32_t>(1000 * (slot + 1)));
        Put32At(dll, taru + kSlotBytes * slot + 4, 50);
    }
    dll[kSkeletonsAt + 12] = 0x40, dll[kSkeletonsAt + 13] = 0x4D; // race 6: 19776
    const auto tables = FindPlayerTables(dll);
    CHECK(tables.has_value());
    PlayerLook look{};
    look[static_cast<size_t>(LookSlot::Face)]  = FaceAndRace(6, 6);
    look[static_cast<size_t>(LookSlot::Head)]  = 0x1000;     // the bare head, its slot in the top bits
    look[static_cast<size_t>(LookSlot::Body)]  = 0x2013;
    look[static_cast<size_t>(LookSlot::Hands)] = 0x0013;
    look[static_cast<size_t>(LookSlot::Main)]  = 0x6000;     // no weapon
    look[static_cast<size_t>(LookSlot::Sub)]   = 0x7002;
    const std::vector<uint32_t> files{19776, 19777, 1006, 2000, 3019, 4019, 5000, 6000, 8002};
    CHECK(PlayerFileIds(*tables, look) == files);

    look[static_cast<size_t>(LookSlot::Feet)] = 60; // past the feet's 50 files
    CHECK(!PlayerFileIds(*tables, look));
    look[static_cast<size_t>(LookSlot::Feet)] = 0;
    look[static_cast<size_t>(LookSlot::Face)] = FaceAndRace(6, 9);
    CHECK(!PlayerFileIds(*tables, look));
}

TEST(a_model_drawn_from_several_files_keeps_the_first_skeleton_and_every_other_part)
{
    ModelFile model, skeleton, upper, body;
    skeleton.skeleton = {{-1, {}, {}}, {0, {}, {}}};
    skeleton.motions  = {Motion{"idl0", 1, 1.0f, {}}, Motion{"run0", 1, 1.0f, {}}};
    upper.skeleton    = {{-1, {}, {}}};
    upper.motions     = {Motion{"idl1", 1, 1.0f, {}}};
    body.meshes       = {ModelMesh{"tr_b", false, {}, {}}};
    body.pictures     = {ColorPicture{"tim     tr_b", 4, 4, {}}};
    for (ModelFile* part : {&skeleton, &upper, &body})
        AddModelFile(model, *part);
    CHECK_EQ(model.skeleton.size(), 2u);
    CHECK_EQ(model.meshes.size(), 1u);
    CHECK_EQ(model.pictures.size(), 1u);
    const auto idle = IdleMotions(model);
    CHECK_EQ(idle.size(), 2u);
    CHECK_EQ(idle[1]->name, std::string("idl1"));
}

TEST(the_dll_tables_are_found_by_their_first_values_at_or_past_where_the_client_keeps_its_data)
{
    Bytes early = Dll();
    early[0x100] = 0xA8, early[0x101] = 0x1B; // the face's first values early in the code are not the table
    const auto found = FindPlayerTables(early);
    CHECK(found.has_value());
    CHECK_EQ(PieceFileId(*found, 1, LookSlot::Face, 0).value_or(0), 0x1BA8u);
    CHECK_EQ(SkeletonFileId(*found, 1).value_or(0), 0x1BA0u);

    Bytes none = Dll();
    none[kSkeletonsAt] = 0;
    CHECK(!FindPlayerTables(none));
}

TEST(a_skeleton_reads_each_joints_parent_turn_and_place)
{
    const std::vector<Joint> joints{{-1, {0, 0, 0.6f, 0.8f}, {1, 2, 3}}, {0, {}, {0, 1, 0}}, {1, {}, {0, 0.5f, 0}}};
    const auto read = ReadModelFile(Chunk("skel", kSkeletonChunkType, SkeletonBody(joints)));
    CHECK(read.value.has_value());
    const auto& skeleton = read.value->skeleton;
    CHECK_EQ(skeleton.size(), 3u);
    CHECK_EQ(skeleton[0].parent, -1);
    CHECK_EQ(skeleton[2].parent, 1);
    CHECK(Near(skeleton[0].rotation.z, 0.6f) && Near(skeleton[0].rotation.w, 0.8f));
    CHECK(Near(skeleton[0].translation.z, 3.0f));
    CHECK(Near(skeleton[2].translation.y, 0.5f));
}

TEST(a_joint_whose_parent_comes_after_it_is_refused)
{
    Bytes body = SkeletonBody({{-1, {}, {}}, {0, {}, {}}});
    body[4] = 1; // the root naming the joint after it
    CHECK(!ReadModelFile(Chunk("skel", kSkeletonChunkType, body)).value);
}

TEST(a_mesh_reads_its_vertices_of_one_and_two_joints_and_their_mirrors)
{
    const std::vector<float> vertices{
        1, 2, 3, 0, 0, 1,                                   // one joint: place, normal
        10, 20, 30, 40, 50, 60, 0.25f, 0.75f, 0, 9, 1, 9, 0, 9, // two joints: x0 x1 y0 y1 z0 z1, w0 w1, normals paired
    };
    const Bytes commands = TextureCommand("tim     skin") + ListCommand({{{{0, 0.1f, 0.2f}, {1, 0.3f, 0.4f}, {0, 0.5f, 0.6f}}}}) + EndCommand();
    const Bytes body = MeshBody(0, true, {}, 1, 1, {{Ref(2, 3, 1), 0}, {Ref(4, 5, 2), Ref(6, 7, 3)}}, vertices, commands);
    const auto read = ReadModelFile(Chunk("body", kMeshChunkType, body));
    CHECK(read.value.has_value());
    const ModelMesh& mesh = read.value->meshes.at(0);
    CHECK_EQ(mesh.name, std::string("body"));
    CHECK(mesh.mirrored);
    CHECK_EQ(mesh.vertices.size(), 2u);
    const ModelVertex& one = mesh.vertices[0];
    CHECK(!one.twoJoints && one.joints[0] == 2 && one.mirrorJoints[0] == 3 && one.mirrorAxes[0] == MirrorAxis::X);
    CHECK(Near(one.positions[0].z, 3.0f) && Near(one.normal.z, 1.0f) && Near(one.weight, 1.0f));
    const ModelVertex& two = mesh.vertices[1];
    CHECK(two.twoJoints && two.joints[0] == 4 && two.joints[1] == 6 && two.mirrorJoints[1] == 7 && two.mirrorAxes[1] == MirrorAxis::Z);
    CHECK(Near(two.positions[0].x, 10) && Near(two.positions[1].x, 20) && Near(two.positions[0].y, 30) && Near(two.positions[1].z, 60));
    CHECK(Near(two.weight, 0.25f));
    CHECK(Near(two.normal.x, 0) && Near(two.normal.y, 1) && Near(two.normal.z, 0));
}

TEST(a_mesh_with_a_joint_list_names_its_joints_through_it)
{
    const std::vector<float> vertex{0, 0, 0, 0, 0, 1};
    const Bytes body = MeshBody(0x80, false, {17, 40}, 1, 0, {{Ref(1, 0, 0), 0}}, vertex, EndCommand());
    const auto read = ReadModelFile(Chunk("hand", kMeshChunkType, body));
    CHECK_EQ(read.value->meshes.at(0).vertices.at(0).joints[0], 40);
    CHECK_EQ(read.value->meshes.at(0).vertices.at(0).mirrorJoints[0], 17);
}

TEST(a_cloth_mesh_has_no_normals_between_its_vertices)
{
    const std::vector<float> vertices{1, 2, 3, 4, 5, 6};
    const Bytes body = MeshBody(0x01, false, {}, 2, 0, {{0, 0}, {0, 0}}, vertices, EndCommand());
    const ModelMesh mesh = ReadModelFile(Chunk("cape", kMeshChunkType, body)).value->meshes.at(0);
    CHECK(Near(mesh.vertices[1].positions[0].x, 4.0f) && Near(mesh.vertices[1].positions[0].z, 6.0f));
}

TEST(a_mesh_draws_lists_and_strips_with_the_picture_named_before_them)
{
    std::vector<float> vertices;
    for (int i = 0; i < 5; ++i)
        vertices.insert(vertices.end(), {0, 0, 0, 0, 0, 1});
    const Bytes commands = MaterialCommand() + TextureCommand("tim     face") +
                           ListCommand({{{{0, 0, 0}, {1, 1, 0}, {2, 0, 1}}}}) + TextureCommand("tim     hair") +
                           StripCommand({{0, 0, 0}, {1, 1, 0}, {2, 0, 1}, {3, 1, 1}, {3, 1, 1}, {4, 0.5f, 0.5f}}) + EndCommand();
    const std::vector<std::array<uint16_t, 2>> refs(5, {0, 0});
    const auto read = ReadModelFile(Chunk("head", kMeshChunkType, MeshBody(0, false, {}, 5, 0, refs, vertices, commands)));
    CHECK(read.value.has_value());
    const auto& parts = read.value->meshes.at(0).parts;
    CHECK_EQ(parts.size(), 2u);
    CHECK_EQ(parts[0].picture, std::string("tim     face    "));
    CHECK_EQ(parts[1].picture, std::string("tim     hair    "));
    CHECK_EQ(parts[0].triangles.size(), 1u);
    CHECK(Near(parts[0].triangles[0].uv[2], 1.0f) && Near(parts[0].triangles[0].uv[5], 1.0f));
    // The strip 0 1 2 3 3 4 is the triangles 0 1 2, 1 3 2 (every second one turned back), 2 3 3 and 3 4 3 (no area).
    const auto& strip = parts[1].triangles;
    CHECK_EQ(strip.size(), 2u);
    CHECK(strip[0].vertices == (std::array<uint16_t, 3>{0, 1, 2}));
    CHECK(strip[1].vertices == (std::array<uint16_t, 3>{1, 3, 2}));
    CHECK(Near(strip[1].uv[2], 1.0f) && Near(strip[1].uv[3], 1.0f));
}

TEST(a_mesh_with_a_command_it_does_not_know_is_refused)
{
    const std::vector<float> vertex{0, 0, 0, 0, 0, 1};
    Bytes commands;
    Put16(commands, 0x1234);
    const auto read = ReadModelFile(Chunk("odd_", kMeshChunkType, MeshBody(0, false, {}, 1, 0, {{0, 0}}, vertex, commands + EndCommand())));
    CHECK(!read.value);
    CHECK(read.failure.find("odd_") != std::string::npos);
}

TEST(a_mesh_whose_offsets_run_past_its_chunk_is_refused)
{
    Bytes body = MeshBody(0, false, {}, 1, 0, {{0, 0}}, {0, 0, 0, 0, 0, 1}, EndCommand());
    body[30] = 0xF0; // the vertices far past the end
    CHECK(!ReadModelFile(Chunk("long", kMeshChunkType, body)).value);
}

TEST(a_motion_reads_each_channel_as_its_frames_or_one_number)
{
    TrackBytes moving{3, {}, kStill};
    moving.at[4]     = 2 * kRecordFloats;     // its move's x channel: the first frames after the records
    moving.constants = {0, 0, 0, 1, 7, 8, 9, 1, 1, 1};
    TrackBytes skipped{4, {}, kStill};
    skipped.at[0] = -1;
    const std::vector<float> frames{0.5f, 1.5f, 2.5f};
    const auto read = ReadModelFile(Chunk("idl0", kMotionChunkType, MotionBody(3, 0.45f, {moving, skipped}, frames)));
    CHECK(read.value.has_value());
    const Motion& motion = read.value->motions.at(0);
    CHECK_EQ(motion.name, std::string("idl0"));
    CHECK_EQ(motion.frames, 3);
    CHECK(Near(motion.speed, 0.45f));
    CHECK_EQ(motion.tracks.size(), 1u);
    const JointTrack& track = motion.tracks[0];
    CHECK_EQ(track.joint, 3);
    CHECK(Near(track.At(4, 0), 0.5f) && Near(track.At(4, 2), 2.5f));
    CHECK(Near(track.At(5, 0), 8.0f) && Near(track.At(5, 2), 8.0f));
    CHECK(Near(track.At(3, 1), 1.0f));
}

TEST(a_motion_whose_frames_run_past_its_chunk_is_refused)
{
    TrackBytes track{0, {}, kStill};
    track.at[0] = 900;
    CHECK(!ReadModelFile(Chunk("idl0", kMotionChunkType, MotionBody(4, 1.0f, {track}, {}))).value);
}

TEST(a_palette_picture_reads_its_rows_from_the_bottom)
{
    // 0xB1: the header, four bytes, 256 BGRA colors, then a byte a pixel with the last row first.
    Bytes body = Picture("tim     eyes    ", 2, 2, "    ", {}, 0xB1);
    body.resize(1 + 16 + 40); // the header alone, without the code and sizes of a DXT picture
    body[1 + 16 + 14] = 8;    // biBitCount
    body.resize(body.size() + 4, 0);
    Bytes palette(256 * 4, 0);
    palette[4 * 1 + 2] = 0xFF, palette[4 * 1 + 3] = 0x80; // color 1: red, the game's opaque
    palette[4 * 2 + 0] = 0xFF, palette[4 * 2 + 3] = 0x40; // color 2: blue, half
    body = body + palette + Bytes{1, 1, 2, 2};
    const auto read = ReadModelFile(Chunk("eyes", kPictureChunkType, body));
    CHECK(read.value.has_value());
    const ColorPicture& picture = read.value->pictures.at(0);
    CHECK_EQ(picture.width, 2);
    CHECK_EQ(picture.bgra[0], 0xFF); // top left: the stored last row, blue
    CHECK_EQ(picture.bgra[3], 0x40);
    CHECK_EQ(picture.bgra[2 * 4 * 2 - 2], 0xFF); // bottom right: red
    CHECK_EQ(picture.bgra[2 * 4 * 2 - 1], 0x80);
}

TEST(a_model_file_keeps_its_parts_in_the_files_order_across_directories)
{
    const Bytes dat = Chunk("mdl_", 0x01, {}) + Chunk("skel", kSkeletonChunkType, SkeletonBody({{-1, {}, {}}})) +
                      Chunk("hf_a", kMeshChunkType, MeshBody(0, false, {}, 1, 0, {{0, 0}}, {0, 0, 0, 0, 0, 1}, EndCommand())) +
                      Chunk("hf_b", kMeshChunkType, MeshBody(0, false, {}, 1, 0, {{0, 0}}, {0, 0, 0, 0, 0, 1}, EndCommand())) +
                      Chunk("end_", 0x00, {}) + Chunk("mot_", 0x01, {}) +
                      Chunk("idl0", kMotionChunkType, MotionBody(1, 1.0f, {{0, {}, kStill}}, {})) + Chunk("end_", 0x00, {});
    const auto read = ReadModelFile(dat);
    CHECK(read.value.has_value());
    CHECK_EQ(read.value->skeleton.size(), 1u);
    CHECK_EQ(read.value->meshes.size(), 2u);
    CHECK_EQ(read.value->meshes[1].name, std::string("hf_b"));
    CHECK_EQ(read.value->motions.size(), 1u);
}

// Read from the game when it is installed where this machine keeps it; nothing to check without it. The counts are
// the Python reader's of the same file, which the spike drew and looked at.
TEST(the_tonberry_cutters_file_reads_as_the_spike_read_it)
{
    const Bytes dat = GameFile("ROM2\\22\\48.DAT");
    if (dat.empty()) return;
    const auto read = ReadModelFile(dat);
    CHECK(read.value.has_value());
    const ModelFile& model = *read.value;
    CHECK_EQ(model.skeleton.size(), 64u);
    CHECK_EQ(model.meshes.size(), 5u);
    size_t triangles = 0, mirrored = 0;
    for (const ModelMesh& mesh : model.meshes)
        for (const ModelPart& part : mesh.parts)
            (mesh.mirrored ? mirrored : triangles) += part.triangles.size();
    CHECK_EQ(triangles, 6u + 110u + 83u);
    CHECK_EQ(mirrored, 650u + 213u);
    CHECK_EQ(model.pictures.size(), 2u);
    CHECK_EQ(model.pictures[0].width, 256);
    CHECK_EQ(model.motions.size(), 19u);
    const Motion* idle = nullptr;
    for (const Motion& motion : model.motions)
        if (motion.name == "idl0") idle = &motion;
    CHECK(idle != nullptr && idle->frames == 16 && idle->tracks.size() == 62u);
}

// Posed at the first frame of its idle as the Python reader posed it for the spike's pictures: the same corners, and
// the same box and middle to a thousandth. The game's up is -y; that file turned it to +y.
TEST(the_tonberry_cutter_stands_in_its_idle_as_the_spike_drew_it)
{
    const Bytes dat = GameFile("ROM2\\22\\48.DAT");
    if (dat.empty()) return;
    const ModelFile model = *ReadModelFile(dat).value;
    const auto batches = PosedBatches(model.meshes, PoseSkeleton(model.skeleton, IdleMotions(model), 0.0f));
    std::array<float, 3> low{1e9f, 1e9f, 1e9f}, high{-1e9f, -1e9f, -1e9f}, sum{};
    size_t corners = 0;
    for (const DrawBatch& batch : batches)
        for (const DrawVertex& v : batch.vertices)
        {
            const std::array<float, 3> p{v.x, v.y, v.z};
            for (size_t k = 0; k < 3; ++k)
            {
                low[k]  = std::min(low[k], p[k]);
                high[k] = std::max(high[k], p[k]);
                sum[k] += p[k];
            }
            ++corners;
        }
    CHECK_EQ(corners, 1925u * 3);
    const auto near = [](float a, float b) { return std::fabs(a - b) < 1e-3f; };
    CHECK(near(low[0], -0.74617f) && near(high[0], 0.60585f) && near(sum[0] / static_cast<float>(corners), 0.110819f));
    CHECK(near(low[1], -0.97295f) && near(high[1], 0.00131f) && near(sum[1] / static_cast<float>(corners), -0.442821f));
    CHECK(near(low[2], -0.42164f) && near(high[2], 0.65171f) && near(sum[2] / static_cast<float>(corners), 0.011838f));
}

namespace
{
    // The bounds and middle of a posed model's corners, axis by axis, and how many there are.
    struct Spread
    {
        std::array<float, 3> low{1e9f, 1e9f, 1e9f}, high{-1e9f, -1e9f, -1e9f}, middle{};
        size_t corners = 0;
    };
    Spread SpreadOf(const std::vector<DrawBatch>& batches)
    {
        Spread out;
        for (const DrawBatch& batch : batches)
            for (const DrawVertex& v : batch.vertices)
            {
                const std::array<float, 3> p{v.x, v.y, v.z};
                for (size_t k = 0; k < 3; ++k)
                {
                    out.low[k]  = std::min(out.low[k], p[k]);
                    out.high[k] = std::max(out.high[k], p[k]);
                    out.middle[k] += p[k];
                }
                ++out.corners;
            }
        for (float& m : out.middle)
            m /= static_cast<float>(std::max<size_t>(out.corners, 1));
        return out;
    }
}

// Shantotto as the server sends her look, found through the install's own tables and FFXiMain.dll, and posed in her
// idle as the spike drew her: the files the spike found, and the same box and middle to a thousandth (-y up).
TEST(shantotto_is_found_and_stands_as_the_spike_drew_her)
{
    FileTables tables;
    for (int folder = 1; folder <= kRomFolders; ++folder)
    {
        tables[static_cast<size_t>(folder - 1)].vtable = GameFile(VTableFile(folder));
        tables[static_cast<size_t>(folder - 1)].ftable = GameFile(FTableFile(folder));
    }
    const Bytes dll = FileBytes(GameFolder() + "FFXiMain.dll");
    if (tables[0].vtable.empty() || dll.empty()) return;
    CHECK_EQ(DatFileOf(tables, 2502).value_or(""), std::string("ROM2\\22\\48.DAT"));
    const auto players = FindPlayerTables(dll);
    CHECK(players.has_value());
    const PlayerLook shantotto{0x0606, 0x1000, 0x2013, 0x3013, 0x4013, 0x5013, 0x6000, 0x7000, 0};
    const auto files = PlayerFileIds(*players, shantotto);
    CHECK(files.has_value());
    CHECK(*files == (std::vector<uint32_t>{19776, 19777, 22958, 19816, 20091, 20347, 20603, 20859}));
    ModelFile model;
    for (uint32_t id : *files)
    {
        const auto file = DatFileOf(tables, id);
        CHECK(file.has_value());
        auto part = ReadModelFile(GameFile(*file));
        CHECK(part.value.has_value());
        AddModelFile(model, std::move(*part.value));
    }
    CHECK_EQ(model.skeleton.size(), 93u);
    const Spread s = SpreadOf(PosedBatches(model.meshes, PoseSkeleton(model.skeleton, IdleMotions(model), 0.0f)));
    CHECK_EQ(s.corners, 2008u * 3);
    const auto near = [](float a, float b) { return std::fabs(a - b) < 1e-3f; };
    CHECK(near(s.low[0], -0.14806f) && near(s.high[0], 0.17645f) && near(s.middle[0], 0.037576f));
    CHECK(near(s.low[1], -0.93576f) && near(s.high[1], 0.00305f) && near(s.middle[1], -0.455457f));
    CHECK(near(s.low[2], -0.26667f) && near(s.high[2], 0.30203f) && near(s.middle[2], 0.012655f));
}

TEST(a_model_pictures_alpha_is_doubled_as_the_game_keeps_0x80_as_opaque)
{
    const ColorPicture picture{"tim     skin", 2, 1, {10, 20, 30, 0x80, 1, 2, 3, 0x40}};
    const std::vector<uint8_t> shown = ShownPixels(picture);
    CHECK(shown == (std::vector<uint8_t>{10, 20, 30, 255, 1, 2, 3, 127}));
    CHECK_EQ(ShownPixels({"", 1, 1, {0, 0, 0, 0x88}})[3], 255); // player pictures go a little past it
}

TEST(a_mesh_draws_lists_and_strips_in_their_own_colors_without_a_picture)
{
    std::vector<float> vertices;
    for (int i = 0; i < 5; ++i)
        vertices.insert(vertices.end(), {0, 0, 0, 0, 0, 1});
    const Bytes commands = TextureCommand("tim     face") + ColorListCommand({{{0, 1, 2}, 0x80112233}}) +
                           ColorStripCommand({0, 1, 2, 3, 3, 4}, 0x80445566) + EndCommand();
    const std::vector<std::array<uint16_t, 2>> refs(5, {0, 0});
    const auto read = ReadModelFile(Chunk("eye_", kMeshChunkType, MeshBody(0, false, {}, 5, 0, refs, vertices, commands)));
    CHECK(read.value.has_value());
    const auto& parts = read.value->meshes.at(0).parts;
    CHECK_EQ(parts.size(), 2u);
    CHECK(!parts[0].textured && parts[0].picture.empty());
    CHECK(parts[0].colors == (std::vector<uint32_t>{0x80112233}));
    // The strip 0 1 2 3 3 4, as a textured one: 0 1 2, then 1 3 2 turned back, and the two with no area left out.
    const auto& strip = parts[1];
    CHECK_EQ(strip.triangles.size(), 2u);
    CHECK(strip.triangles[1].vertices == (std::array<uint16_t, 3>{1, 3, 2}));
    CHECK(strip.colors == (std::vector<uint32_t>{0x80445566, 0x80445566}));
}

TEST(a_mesh_naming_a_joint_past_its_list_is_refused)
{
    const Bytes body = MeshBody(0x80, false, {17}, 1, 0, {{Ref(1, 0, 0), 0}}, {0, 0, 0, 0, 0, 1}, EndCommand());
    const auto read = ReadModelFile(Chunk("hand", kMeshChunkType, body));
    CHECK(!read.value);
    CHECK(read.failure.find("joint") != std::string::npos);
}

TEST(a_mesh_drawing_a_vertex_it_does_not_have_is_refused)
{
    const Bytes commands = ListCommand({{{{0, 0, 0}, {1, 0, 0}, {7, 0, 0}}}}) + EndCommand();
    const auto read = ReadModelFile(Chunk("foot", kMeshChunkType, MeshBody(0, false, {}, 2, 0, {{0, 0}, {0, 0}},
        {0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1}, commands)));
    CHECK(!read.value);
    CHECK(read.failure.find("vertex") != std::string::npos);
}

TEST(a_model_file_whose_chunks_run_past_its_end_is_refused)
{
    Bytes dat = Chunk("skel", kSkeletonChunkType, SkeletonBody({{-1, {}, {}}}));
    dat.resize(dat.size() - 16);
    const auto read = ReadModelFile(dat);
    CHECK(!read.value);
    CHECK(read.failure.find("end") != std::string::npos);
}

namespace
{
    // A picture of plain pixels: the header with its bit count, maybe the four bytes of 0xB1, then its pixels.
    Bytes PlainPicture(uint8_t flag, int width, int height, uint16_t bits, const Bytes& pixels)
    {
        Bytes body = Picture("tim     skin    ", width, height, "    ", {}, flag);
        body.resize(1 + 16 + 40);
        body[1 + 16 + 14] = static_cast<uint8_t>(bits);
        if (flag == 0xB1) body.resize(body.size() + 4, 0);
        return body + pixels;
    }
}

TEST(a_picture_of_four_bytes_a_pixel_reads_its_rows_from_the_bottom)
{
    // Two rows of one pixel: the stored first is the bottom one.
    const Bytes pixels{1, 2, 3, 0x80, 9, 8, 7, 0x40};
    const auto read = ReadModelFile(Chunk("skin", kPictureChunkType, PlainPicture(0xB1, 1, 2, 32, pixels)));
    CHECK(read.value.has_value());
    CHECK(read.value->pictures.at(0).bgra == (Bytes{9, 8, 7, 0x40, 1, 2, 3, 0x80}));
}

TEST(a_plain_picture_of_flag_0x91_has_no_gap_before_its_pixels)
{
    const Bytes pixels{5, 6, 7, 0x80};
    const auto read = ReadModelFile(Chunk("skin", kPictureChunkType, PlainPicture(0x91, 1, 1, 32, pixels)));
    CHECK(read.value.has_value());
    CHECK(read.value->pictures.at(0).bgra == pixels);
    CHECK(!ReadModelFile(Chunk("skin", kPictureChunkType, PlainPicture(0x91, 1, 1, 16, {1, 2}))).value); // 16 bits: no such picture
}

// The look's bytes as the server sends them, face then race: "07 03" is face 7 of race 3.
TEST(a_looks_first_word_is_its_face_then_its_race)
{
    CHECK_EQ(FaceAndRace(7, 3), uint16_t{0x0307});
    CHECK_EQ(FaceOf(0x0307), uint8_t{7});
    CHECK_EQ(RaceOf(0x0307), uint8_t{3});
}
