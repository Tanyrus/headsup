#pragma once

#include "Ashita.h"
#include "model_pose.h"

#include <map>
#include <string>
#include <vector>

namespace headsup
{
    // A picture of its own to draw models into, with a depth and stencil surface: a default-pool render target, to
    // let go of before the device's Reset.
    class ModelPicture
    {
      public:
        // Makes it the render target, width by height, cleared to the background (ARGB). It changes the target and
        // the viewport: the caller saves and puts back the game's. False when Direct3D refuses.
        bool Begin(IDirect3DDevice8* device, int width, int height, uint32_t background);
        // The texture, and the share of it drawn into (its sides are powers of two).
        IDirect3DTexture8* Texture() const { return m_Target; }
        float U() const { return m_U; }
        float V() const { return m_V; }
        int Width() const { return m_Width; }
        int Height() const { return m_Height; }
        void Release();

      private:
        IDirect3DTexture8* m_Target = nullptr;
        IDirect3DSurface8* m_Depth  = nullptr;
        int m_Side = 0, m_Width = 0, m_Height = 0;
        float m_U = 1.0f, m_V = 1.0f;
    };

    // An outline as HeadsUp draws one in the world: copies of the model shifted round it by pixels, in a color at an
    // alpha, where the model itself is not.
    struct ModelOutline
    {
        uint32_t argb  = 0;
        uint8_t alpha  = 0xFF;
        float pixels   = 0.0f;
        int copies     = 0;
        uint8_t stencil = 1; // the mark of this model's pixels, other than any other model's in the picture
    };

    // A game model ready to draw: its pictures made into textures.
    class DrawnModel
    {
      public:
        // False when Direct3D refuses one of its pictures.
        bool Load(IDirect3DDevice8* device, ModelFile model);
        bool Loaded() const { return m_Loaded; }
        // Its box as it stands, at its own place.
        const ModelBounds& Standing() const { return m_Bounds; }
        // Its triangles at seconds into its idle, moved by at.
        std::vector<DrawBatch> Posed(double seconds, const Vec3& at) const;
        // The triangles drawn into the picture bound as the target, lit from yaw, with their outline under them when
        // there is one. Changes render states: the caller saves and puts back the game's.
        void Draw(IDirect3DDevice8* device, const std::vector<DrawBatch>& batches, const ModelView& view, float yaw,
            const ModelOutline* outline, int width, int height) const;
        void Release();

      private:
        struct Texture
        {
            IDirect3DTexture8* texture = nullptr;
            float u = 1.0f, v = 1.0f;
        };
        // Each batch with its picture, or untextured.
        void DrawBatches(IDirect3DDevice8* device, const std::vector<DrawBatch>& batches, bool textures) const;

        ModelFile m_Model;
        ModelBounds m_Bounds;
        bool m_Loaded = false;
        std::map<std::string, Texture> m_Textures; // by picture name
    };
}
