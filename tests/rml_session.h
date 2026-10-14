#pragma once

#include "fonts.h"

#include <RmlUi/Core.h>

#include <cstdint>
#include <string>
#include <map>
#include <vector>

namespace test
{
    // RmlUi asks for geometry and textures and is given handles that draw nothing.
    class NoRender : public Rml::RenderInterface
    {
    public:
        Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex>, Rml::Span<const int>) override { return ++m_Next; }
        void RenderGeometry(Rml::CompiledGeometryHandle, Rml::Vector2f, Rml::TextureHandle) override {}
        void ReleaseGeometry(Rml::CompiledGeometryHandle) override {}
        Rml::TextureHandle LoadTexture(Rml::Vector2i& dimensions, const Rml::String&) override
        {
            dimensions = Rml::Vector2i(kImageSide, kImageSide);
            return ++m_Next;
        }
        Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte>, Rml::Vector2i) override { return ++m_Next; }
        void ReleaseTexture(Rml::TextureHandle) override {}
        void EnableScissorRegion(bool) override {}
        void SetScissorRegion(Rml::Rectanglei) override {}
        void SetTransform(const Rml::Matrix4f*) override {}
        // An opacity filter keeps its value; a layer composited records the opacity its filters ask for.
        Rml::CompiledFilterHandle CompileFilter(const Rml::String& name, const Rml::Dictionary& parameters) override
        {
            const auto value = parameters.find("value");
            if (name != "opacity" || value == parameters.end()) return 0;
            m_Filters[++m_Next] = value->second.Get<float>();
            return m_Next;
        }
        void ReleaseFilter(Rml::CompiledFilterHandle filter) override { m_Filters.erase(filter); }
        Rml::LayerHandle PushLayer() override { return ++m_Next; }
        void CompositeLayers(Rml::LayerHandle, Rml::LayerHandle, Rml::BlendMode, Rml::Span<const Rml::CompiledFilterHandle> filters) override
        {
            float opacity = 1.0f;
            for (const Rml::CompiledFilterHandle filter : filters)
                opacity *= m_Filters.at(filter);
            composited.push_back(opacity);
        }
        void PopLayer() override {}
        std::vector<float> composited;

    private:
        std::map<uintptr_t, float> m_Filters;
        static constexpr int kImageSide = 16;
        uintptr_t m_Next                = 0;
    };

    // Keeps what RmlUi warns of: markup it could not read, a property it does not know, a font it lacks. Its clock
    // stands still until a test moves it, so what takes time takes the same time in every run.
    class Complaints : public Rml::SystemInterface
    {
    public:
        std::vector<std::string> said;
        double seconds = 0.0;
        double GetElapsedTime() override { return seconds; }
        bool LogMessage(Rml::Log::Type type, const Rml::String& message) override
        {
            if (type <= Rml::Log::LT_WARNING) said.push_back(message);
            return true;
        }
    };

    // RmlUi running with the bundled fonts and one context, for as long as this lives.
    class RmlSession
    {
    public:
        static constexpr int kWidth = 1920, kHeight = 1080;
        Complaints complaints;
        Rml::Context* context = nullptr;

        RmlSession()
        {
            Rml::SetSystemInterface(&complaints);
            Rml::SetRenderInterface(&m_Render);
            Rml::Initialise();
            size_t count = 0;
            for (const headsup::BundledFont* font = headsup::BundledFonts(count); count-- > 0; ++font)
                Rml::LoadFontFace(Rml::Span<const Rml::byte>(font->data, font->bytes), font->family, Rml::Style::FontStyle::Normal);
            context = Rml::CreateContext("overlay", Rml::Vector2i(kWidth, kHeight));
        }
        ~RmlSession() { Rml::Shutdown(); }
        const NoRender& Render() const { return m_Render; }
        RmlSession(const RmlSession&)            = delete;
        RmlSession& operator=(const RmlSession&) = delete;

        // Every complaint on a line of its own; empty when RmlUi had none.
        std::string Said() const
        {
            std::string all;
            for (const std::string& line : complaints.said)
                all += line + "\n";
            return all;
        }

    private:
        NoRender m_Render;
    };
}
