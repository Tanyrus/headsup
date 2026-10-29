#pragma once

#include "model_render.h"
#include "nameplate_render.h"
#include "rml_render.h"
#include "setup.h"
#include "setup_scene.h"

#include <functional>
#include <optional>
#include <string>

namespace headsup
{
    // Setup's preview, in the window's picture: the scene's models drawn from the game's files, with their outlines,
    // and the nameplates and the cursor drawn by the world's own renderer, as the settings now have them.
    class SetupPreview
    {
      public:
        static constexpr int kWidth = 640, kHeight = 360;
        // A model from the game's files: a monster's by its number, or a player's by their look. None, saying why,
        // when it cannot be read.
        struct Readers
        {
            std::function<std::optional<ModelFile>(uint16_t model, std::string& failure)> npc;
            std::function<std::optional<ModelFile>(const PlayerLook& look, std::string& failure)> player;
        };
        void SetDevice(IDirect3DDevice8* device);
        // The scene's own nameplates, which take the game's font, the pearl and a cursor file as the world's do.
        NameplateRenderer& Plates() { return m_Plates; }
        // The scene of a step at now drawn into the picture, zoom percent near: you as yourName and yourLook have you.
        // Changes the render target and states: the caller saves and puts back the game's. False while a model could
        // not be read or drawn.
        bool Render(const Settings& settings, SetupStep step, int zoom, double now, const std::string& yourName,
            const PlayerLook& yourLook, const Readers& read);
        std::optional<RmlRender::LivePicture> Picture() const;
        // Why a model could not be read or drawn, once.
        std::string TakeFailure() { return std::exchange(m_Failure, {}); }
        void ReleaseTarget() { m_Picture.Release(); }
        void Release();

      private:
        // A model of the scene, read the first time it is wanted; once it fails it is not tried again.
        struct Sample
        {
            DrawnModel model;
            bool tried = false;
            PlayerLook look{}; // a player's, read again when it changes
        };
        bool Ready(Sample& sample, const std::function<std::optional<ModelFile>(std::string&)>& read);
        void Opaque();

        IDirect3DDevice8* m_Device = nullptr;
        ModelPicture m_Picture;
        Sample m_Tonberry, m_You, m_Shantotto, m_Adventurer;
        NameplateRenderer m_Plates;
        Tracker m_Tracker;
        bool m_Drawn = false; // the picture holds a scene
        std::string m_Failure;
    };
}
