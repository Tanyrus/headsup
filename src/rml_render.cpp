#include "rml_render.h"

#include "gpu.h"

#include <utility>

namespace headsup
{
    namespace
    {
        constexpr int kVerticesPerTriangle = 3;
    }

    void RmlRender::Begin(int width, int height)
    {
        m_Width  = width;
        m_Height = height;
        if (m_Device == nullptr) return;
        m_Device->GetViewport(&m_GameViewport);
        SetQuadStates(m_Device, false);
        // RmlUi's colors and textures carry premultiplied alpha.
        m_Device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
        m_Textured = true;
        m_Transform.reset();
        m_Masked = false;
        ApplyScissor();
    }

    void RmlRender::End()
    {
        if (m_Device == nullptr) return;
        m_Device->SetTexture(0, nullptr);
        m_Device->SetViewport(&m_GameViewport);
    }

    Rml::CompiledGeometryHandle RmlRender::CompileGeometry(Rml::Span<const Rml::Vertex> vertices,
        Rml::Span<const int> indices)
    {
        // A plain triangle list: an indexed draw from user memory would leave the game's index buffer unbound.
        auto* geometry = new Geometry;
        geometry->vertices.reserve(indices.size());
        for (const int index : indices)
        {
            const Rml::Vertex& v = vertices[static_cast<size_t>(index)];
            geometry->vertices.push_back(PlacedVertex(v.position.x, v.position.y, v.colour.red, v.colour.green,
                v.colour.blue, v.colour.alpha, v.tex_coord.x, v.tex_coord.y, 0.0f, 0.0f, 1.0f, 1.0f));
        }
        return reinterpret_cast<Rml::CompiledGeometryHandle>(geometry);
    }

    void RmlRender::RenderGeometry(Rml::CompiledGeometryHandle handle, Rml::Vector2f translation,
        Rml::TextureHandle textureHandle)
    {
        if (m_Device == nullptr || handle == 0 || m_NothingVisible) return;
        const Geometry& geometry = *reinterpret_cast<const Geometry*>(handle);
        if (geometry.vertices.empty()) return;
        const Texture* texture   = reinterpret_cast<const Texture*>(textureHandle);
        const float shareU = texture != nullptr ? texture->u : 1.0f, shareV = texture != nullptr ? texture->v : 1.0f;
        m_Placed.assign(geometry.vertices.begin(), geometry.vertices.end());
        for (ScreenVertex& v : m_Placed)
        {
            v.x += translation.x;
            v.y += translation.y;
            if (m_Transform)
            {
                // The transform is of pixels' corners; the vertices are kept at their centers.
                v.x += kPixelCenter;
                v.y += kPixelCenter;
                Transformed(m_Transform->data(), v.x, v.y);
                v.x -= kPixelCenter;
                v.y -= kPixelCenter;
            }
            v.u *= shareU;
            v.v *= shareV;
        }
        const bool textured = texture != nullptr;
        if (textured != m_Textured)
        {
            m_Textured = textured;
            m_Device->SetTextureStageState(0, D3DTSS_COLOROP, textured ? D3DTOP_MODULATE : D3DTOP_SELECTARG2);
            m_Device->SetTextureStageState(0, D3DTSS_ALPHAOP, textured ? D3DTOP_MODULATE : D3DTOP_SELECTARG2);
        }
        m_Device->SetTexture(0, textured ? texture->texture : nullptr);
        m_Device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, static_cast<UINT>(m_Placed.size() / kVerticesPerTriangle),
            m_Placed.data(), sizeof(ScreenVertex));
    }

    void RmlRender::ReleaseGeometry(Rml::CompiledGeometryHandle handle)
    {
        delete reinterpret_cast<Geometry*>(handle);
    }

    Rml::TextureHandle RmlRender::Upload(const uint32_t* argb, int width, int height)
    {
        if (m_Device == nullptr) return 0;
        auto* texture    = new Texture;
        texture->texture = CreateTexture(m_Device, argb, width, height, texture->u, texture->v);
        if (texture->texture == nullptr)
        {
            delete texture;
            m_Failure = "Direct3D could not make a " + std::to_string(width) + "x" + std::to_string(height) + " texture";
            return 0;
        }
        return reinterpret_cast<Rml::TextureHandle>(texture);
    }

    Rml::TextureHandle RmlRender::LoadTexture(Rml::Vector2i& dimensions, const Rml::String& source)
    {
        if (!m_Images) return 0;
        const std::optional<Image> image = m_Images(source);
        if (!image || image->width <= 0 || image->height <= 0) return 0;
        m_Pixels.resize(image->argb.size());
        for (size_t i = 0; i < image->argb.size(); ++i)
            m_Pixels[i] = Premultiplied(image->argb[i]);
        dimensions = Rml::Vector2i(image->width, image->height);
        return Upload(m_Pixels.data(), image->width, image->height);
    }

    Rml::TextureHandle RmlRender::GenerateTexture(Rml::Span<const Rml::byte> source, Rml::Vector2i dimensions)
    {
        if (dimensions.x <= 0 || dimensions.y <= 0) return 0;
        const auto pixels = static_cast<size_t>(dimensions.x) * static_cast<size_t>(dimensions.y);
        if (source.size() < pixels * kBytesPerPixel) return 0;
        m_Pixels.resize(pixels);
        ArgbFromRgba(source.data(), pixels, m_Pixels.data());
        return Upload(m_Pixels.data(), dimensions.x, dimensions.y);
    }

    void RmlRender::ReleaseTexture(Rml::TextureHandle handle)
    {
        auto* texture = reinterpret_cast<Texture*>(handle);
        if (texture == nullptr) return;
        headsup::ReleaseTexture(texture->texture);
        delete texture;
    }

    void RmlRender::EnableScissorRegion(bool enable)
    {
        m_Scissor = enable;
        ApplyScissor();
    }

    void RmlRender::SetScissorRegion(Rml::Rectanglei region)
    {
        m_Region = region;
        if (m_Scissor) ApplyScissor();
    }

    void RmlRender::SetTransform(const Rml::Matrix4f* transform)
    {
        if (transform != nullptr)
            m_Transform = *transform;
        else
            m_Transform.reset();
    }

    void RmlRender::EnableClipMask(bool enable)
    {
        m_Masked = enable;
        ApplyScissor();
    }

    void RmlRender::RenderToClipMask(Rml::ClipMaskOperation operation, Rml::CompiledGeometryHandle handle,
        Rml::Vector2f translation)
    {
        if (handle == 0) return;
        const Geometry& geometry = *reinterpret_cast<const Geometry*>(handle);
        const PixelEdges edges = MaskEdges(geometry.vertices.data(), geometry.vertices.size(), translation.x,
            translation.y, m_Transform ? m_Transform->data() : nullptr);
        // What lies outside a shape is no rectangle: that mask clips nothing.
        if (operation == Rml::ClipMaskOperation::SetInverse)
            m_Mask = PixelEdges{0, 0, m_Width, m_Height};
        else
            m_Mask = operation == Rml::ClipMaskOperation::Intersect ? Shared(m_Mask, edges) : edges;
        ApplyScissor();
    }

    void RmlRender::ApplyScissor()
    {
        if (m_Device == nullptr) return;
        D3DVIEWPORT8 viewport = m_GameViewport;
        PixelEdges edges = m_Scissor ? PixelEdges{m_Region.Left(), m_Region.Top(), m_Region.Right(), m_Region.Bottom()}
                                     : PixelEdges{0, 0, m_Width, m_Height};
        if (m_Masked) edges = Shared(edges, m_Mask);
        const std::optional<PixelRect> rect = ClampedRect(edges.left, edges.top, edges.right, edges.bottom, m_Width, m_Height);
        m_NothingVisible = !rect;
        if (!rect) return;
        viewport.X      = static_cast<DWORD>(rect->x);
        viewport.Y      = static_cast<DWORD>(rect->y);
        viewport.Width  = static_cast<DWORD>(rect->width);
        viewport.Height = static_cast<DWORD>(rect->height);
        m_Device->SetViewport(&viewport);
    }

    std::string RmlRender::TakeFailure() { return std::exchange(m_Failure, std::string()); }
}
