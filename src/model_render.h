#pragma once

#include "Ashita.h"
#include "model_pose.h"

#include <map>
#include <string>

namespace headsup
{
    // Draws a game model, posed in its idle, into a picture of its own with a depth and stencil surface, for a window
    // to show. The picture is a default-pool render target: let go of it before the device's Reset.
    class ModelRenderer
    {
      public:
        // Takes the model and makes its pictures into textures. False when Direct3D refuses one.
        bool Load(IDirect3DDevice8* device, ModelFile model);
        bool Loaded() const { return m_Loaded; }
        // The model at seconds into its idle, seen from yaw radians round it, into a picture width by height. Changes
        // the render target and every state the drawing needs: the caller saves and puts back the game's.
        bool Render(IDirect3DDevice8* device, double seconds, float yaw, int width, int height);
        // The picture, and the share of it drawn into (its sides are powers of two).
        IDirect3DTexture8* Picture() const { return m_Target; }
        float PictureU() const { return m_U; }
        float PictureV() const { return m_V; }
        void ReleaseTarget();
        void Release();

      private:
        bool EnsureTarget(IDirect3DDevice8* device, int width, int height);

        ModelFile m_Model;
        ModelBounds m_Bounds;
        bool m_Loaded = false;
        struct Texture
        {
            IDirect3DTexture8* texture = nullptr;
            float u = 1.0f, v = 1.0f;
        };
        std::map<std::string, Texture> m_Textures; // by picture name
        IDirect3DTexture8* m_Target = nullptr;
        IDirect3DSurface8* m_Depth  = nullptr;
        int m_Side = 0;
        float m_U = 1.0f, m_V = 1.0f;
    };
}
