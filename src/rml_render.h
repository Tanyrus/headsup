#pragma once

#include "Ashita.h"
#include "plate_image.h"
#include "rml_pure.h"

#include <RmlUi/Core/RenderInterface.h>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace headsup
{
    // RmlUi's drawing on the Direct3D 8 device: compiled geometry kept as pretransformed vertices and drawn with its
    // translation added, textures padded to powers of two as the plates' are, premultiplied alpha, and the scissor
    // and the clip mask as the viewport, which Direct3D 8 clips pretransformed vertices to.
    class RmlRender : public Rml::RenderInterface
    {
    public:
        // The images documents name as hu: sources: straight-alpha ARGB, or nothing for a name no one serves.
        using ImageSource = std::function<std::optional<Image>(const std::string& name)>;

        void SetDevice(IDirect3DDevice8* device) { m_Device = device; }
        void SetImageSource(ImageSource source) { m_Images = std::move(source); }
        // Around a context's Render: the quad states with premultiplied blending and the full viewport, the game's
        // viewport put back after.
        void Begin(int width, int height);
        void End();

        Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex> vertices,
            Rml::Span<const int> indices) override;
        void RenderGeometry(Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation,
            Rml::TextureHandle texture) override;
        void ReleaseGeometry(Rml::CompiledGeometryHandle geometry) override;
        Rml::TextureHandle LoadTexture(Rml::Vector2i& dimensions, const Rml::String& source) override;
        Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte> source, Rml::Vector2i dimensions) override;
        void ReleaseTexture(Rml::TextureHandle texture) override;
        void EnableScissorRegion(bool enable) override;
        void SetScissorRegion(Rml::Rectanglei region) override;
        // Transforms move the vertices as they are placed: turned, scaled and sliding elements, with nothing more
        // asked of Direct3D.
        void SetTransform(const Rml::Matrix4f* transform) override;
        // RmlUi clips what scrolls under a transform by a mask and no longer by the scissor: without these a list
        // that drops in spills over its box until it lands. The mask is kept as a rectangle (MaskEdges).
        void EnableClipMask(bool enable) override;
        void RenderToClipMask(Rml::ClipMaskOperation operation, Rml::CompiledGeometryHandle geometry,
            Rml::Vector2f translation) override;
        // An element with a filter is drawn into a layer, a picture of the whole target, and laid over the layer under
        // it when it ends; of the filters, opacity alone. A layer Direct3D refuses leaves the element drawn straight
        // on, at full opacity.
        Rml::LayerHandle PushLayer() override;
        void CompositeLayers(Rml::LayerHandle source, Rml::LayerHandle destination, Rml::BlendMode blendMode,
            Rml::Span<const Rml::CompiledFilterHandle> filters) override;
        void PopLayer() override;
        Rml::CompiledFilterHandle CompileFilter(const Rml::String& name, const Rml::Dictionary& parameters) override;
        void ReleaseFilter(Rml::CompiledFilterHandle filter) override;
        // Layers are default-pool render targets, kept from frame to frame: they must be let go before the device's
        // Reset, or the game's Reset fails.
        void ReleaseLayers();

        // A texture Direct3D refused, once.
        std::string TakeFailure();

    private:
        struct Geometry
        {
            std::vector<ScreenVertex> vertices; // a triangle list
        };
        struct Texture
        {
            IDirect3DTexture8* texture = nullptr;
            float u = 0.0f, v = 0.0f; // the image's share of the padded texture
        };

        struct Layer
        {
            IDirect3DTexture8* texture = nullptr;
            IDirect3DSurface8* surface = nullptr; // its picture, as a render target
            int side[2]                = {0, 0};  // padded width and height
        };

        Rml::TextureHandle Upload(const uint32_t* argb, int width, int height);
        void ApplyScissor();
        bool MakeLayer(size_t index);
        // Draws into the layer at this depth of the stack, or the nearest one under it that Direct3D gave; 0 is the
        // game's target.
        void Bind(size_t depth);
        void UseTexture(bool textured);

        IDirect3DDevice8* m_Device = nullptr;
        ImageSource m_Images;
        int m_Width = 0, m_Height = 0;
        bool m_Scissor = false;
        Rml::Rectanglei m_Region;
        bool m_Masked = false;
        PixelEdges m_Mask;
        bool m_NothingVisible = false; // the scissor and the mask leave no pixel: nothing is drawn
        D3DVIEWPORT8 m_GameViewport{};
        bool m_Textured = true; // the color stage reads the texture; untextured geometry takes its vertex colors
        std::optional<Rml::Matrix4f> m_Transform;
        std::vector<ScreenVertex> m_Placed;
        std::vector<uint32_t> m_Pixels;
        std::string m_Failure;
        std::vector<Layer> m_Layers;   // one for each depth the stack has reached
        std::vector<bool> m_Stack;     // the layers pushed: false for one Direct3D refused
        bool m_LayersRefused = false;  // not asked again until the layers are let go
        IDirect3DSurface8* m_GameTarget = nullptr; // while a layer is drawn into this frame
        IDirect3DSurface8* m_GameDepth  = nullptr;
    };
}
