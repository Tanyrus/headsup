#pragma once

#include "game_font.h"
#include "player_status.h"

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace headsup
{
    // The game finds a file by its id in nine pairs of tables, one a ROM folder: VTABLE.DAT and FTABLE.DAT for ROM,
    // ROMn\VTABLEn.DAT and ROMn\FTABLEn.DAT for ROM2 to ROM9. A folder's VTABLE holds its own number at the ids it
    // has, and its FTABLE their place: the folder under it (the high 9 bits) and the file in that (the low 7).
    constexpr int kRomFolders = 9;
    struct RomTables
    {
        std::vector<uint8_t> vtable, ftable; // the two files whole
    };
    using FileTables = std::array<RomTables, kRomFolders>;
    // The files of a ROM folder's pair, under the game's folder, for folder 1 to kRomFolders.
    std::string VTableFile(int folder);
    std::string FTableFile(int folder);
    // The file of an id under the game's folder ("ROM2\\22\\48.DAT"), or none for an id no folder has.
    std::optional<std::string> DatFileOf(const FileTables& tables, uint32_t fileId);

    // The file id of a monster's or NPC's model, from its number in the look the server sends. Not every number has
    // a file, and chairs, mounts and chocobos are found another way.
    uint32_t NpcModelFileId(uint16_t model);

    // What a character wears, slot by slot, as a player's look gives each piece.
    enum class LookSlot : uint8_t { Face, Head, Body, Hands, Legs, Feet, Main, Sub, Range };
    constexpr int kLookSlots = static_cast<int>(LookSlot::Range) + 1;
    static_assert(kLookSlots == kModelParts, "a look's parts are its slots");
    constexpr int kPlayerRaces = 8; // Hume male to Galka, 1 to 8
    // FFXiMain.dll's tables of where a race's files are: for each race and slot, up to six runs of file ids that a
    // piece's model number counts along, and the file of the race's skeleton and its lower body's animations (its
    // upper body's are the next file).
    struct PlayerTables
    {
        struct Run
        {
            uint32_t first = 0, count = 0;
        };
        static constexpr int kRuns = 6;
        std::array<std::array<std::array<Run, kRuns>, kLookSlots>, kPlayerRaces> pieces{};
        std::array<uint16_t, kPlayerRaces> skeletons{};
    };
    // The tables out of the DLL's bytes, found by their first values; none when either is missing.
    std::optional<PlayerTables> FindPlayerTables(std::span<const uint8_t> dll);
    // The file of a race's piece in a slot, or none past the race's runs. race is 1 to kPlayerRaces.
    std::optional<uint32_t> PieceFileId(const PlayerTables& tables, int race, LookSlot slot, uint16_t model);
    std::optional<uint32_t> SkeletonFileId(const PlayerTables& tables, int race);

    // The files a player is drawn from: the skeleton's, which holds the lower body's animations, the next one, which
    // holds the upper body's, then the face's and each piece's. The look is by LookSlot, its first word the face and
    // the race (1 to kPlayerRaces) and each piece's model maybe carrying its slot in its top 4 bits. A weapon of model
    // 0 is none. None for a race that is not a player's or a piece past its race's files.
    std::optional<std::vector<uint32_t>> PlayerFileIds(const PlayerTables& tables, const PlayerLook& look);
    // A look's first word, as the server packs it: the face in its low byte, the race in its high one.
    constexpr int kRaceShift = 8;
    constexpr uint16_t FaceAndRace(uint8_t face, uint8_t race) { return static_cast<uint16_t>(race << kRaceShift | face); }
    constexpr uint8_t FaceOf(uint16_t faceAndRace) { return static_cast<uint8_t>(faceAndRace); }
    constexpr uint8_t RaceOf(uint16_t faceAndRace) { return static_cast<uint8_t>(faceAndRace >> kRaceShift); }

    struct Vec3
    {
        float x = 0.0f, y = 0.0f, z = 0.0f;
    };
    struct Quat
    {
        float x = 0.0f, y = 0.0f, z = 0.0f, w = 1.0f;
    };

    // A joint of a skeleton: its parent (-1 for the root, which is its own in the file), and its turn and place in
    // the parent's space as the model stands.
    struct Joint
    {
        int parent = -1;
        Quat rotation;
        Vec3 translation;
    };

    // A vertex of a skinned mesh holds itself in the space of each of its one or two joints, already weighted: it is
    // where the first joint puts positions[0] with its place counted weight times, plus where the second puts
    // positions[1] with its place counted the rest. A mirrored mesh draws each vertex again by the mirror joints with
    // one axis of each position turned around, numbered as the file numbers them.
    enum class MirrorAxis : uint8_t { None, X, Y, Z };
    struct ModelVertex
    {
        std::array<uint8_t, 2> joints{}, mirrorJoints{};
        std::array<MirrorAxis, 2> mirrorAxes{};
        std::array<Vec3, 2> positions{};
        float weight   = 1.0f; // 1 for a vertex of one joint
        bool twoJoints = false;
        Vec3 normal;
    };
    struct ModelTriangle
    {
        std::array<uint16_t, 3> vertices{};
        std::array<float, 6> uv{}; // u and v of each corner
    };
    // Triangles drawn with one picture, named as the picture chunk is; or, with no picture, in one color each.
    struct ModelPart
    {
        std::string picture;
        bool textured = true;
        std::vector<ModelTriangle> triangles;
        std::vector<uint32_t> colors; // ARGB a triangle, for a part without a picture
    };
    struct ModelMesh
    {
        std::string name;
        bool mirrored = false;
        std::vector<ModelVertex> vertices;
        std::vector<ModelPart> parts;
    };

    // An animation: for each joint it moves, a turn, a move and a size (x, y, z, w; x, y, z; x, y, z), each of its
    // ten numbers one for every frame or the same throughout. Played at speed frames a 30th of a second.
    struct JointTrack
    {
        int joint = 0;
        static constexpr int kChannels = 10;
        std::array<std::vector<float>, kChannels> channels;
        float At(int channel, int frame) const;
    };
    struct Motion
    {
        std::string name; // its chunk's, such as "idl0"
        int frames = 0;
        float speed = 0.0f;
        std::vector<JointTrack> tracks;
    };

    // A model's file: its skeleton (a player's pieces have none), meshes, pictures and animations, each read in the
    // file's order. A picture's alpha is as the game keeps it, where 0x80 is opaque.
    struct ModelFile
    {
        std::vector<Joint> skeleton;
        std::vector<ModelMesh> meshes;
        std::vector<ColorPicture> pictures;
        std::vector<Motion> motions;
    };
    Read<ModelFile> ReadModelFile(std::span<const uint8_t> dat);
    // A part's file added to a model drawn from several: the first skeleton read, and every mesh, picture and motion.
    void AddModelFile(ModelFile& model, ModelFile part);
    // The motions a model stands still with: a monster's, or a player's lower body and upper body.
    constexpr std::string_view kIdleMotions[]{"idl0", "idl1"};
    std::vector<const Motion*> IdleMotions(const ModelFile& model);

    // A model's alpha as Direct3D draws it: the game keeps 0x80 as opaque, in its pictures and its colors alike, so
    // each is doubled.
    uint8_t ShownAlpha(uint8_t gameAlpha);
    uint32_t ShownColor(uint32_t gameArgb);
    std::vector<uint8_t> ShownPixels(const ColorPicture& picture);

    // The chunk types of a model's file.
    constexpr uint8_t kSkeletonChunkType = 0x29, kMeshChunkType = 0x2A, kMotionChunkType = 0x2B;
}
