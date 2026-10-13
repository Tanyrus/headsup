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
    // as the viewport, which Direct3D 8 clips pretransformed vertices to.
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
        // asked of Direct3D. What a transformed element clips is not clipped.
        void SetTransform(const Rml::Matrix4f* transform) override;

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

        Rml::TextureHandle Upload(const uint32_t* argb, int width, int height);
        void ApplyScissor();

        IDirect3DDevice8* m_Device = nullptr;
        ImageSource m_Images;
        int m_Width = 0, m_Height = 0;
        bool m_Scissor = false;
        Rml::Rectanglei m_Region;
        bool m_NothingVisible = false; // the scissor leaves no pixel: nothing is drawn
        D3DVIEWPORT8 m_GameViewport{};
        bool m_Textured = true; // the color stage reads the texture; untextured geometry takes its vertex colors
        std::optional<Rml::Matrix4f> m_Transform;
        std::vector<ScreenVertex> m_Placed;
        std::vector<uint32_t> m_Pixels;
        std::string m_Failure;
    };
}
