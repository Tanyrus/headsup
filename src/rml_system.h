#pragma once

#include <RmlUi/Core/SystemInterface.h>

#include <chrono>
#include <functional>
#include <string>

namespace headsup
{
    // What RmlUi asks of the host besides drawing: the clock, the clipboard, and where its warnings go.
    class RmlSystem : public Rml::SystemInterface
    {
    public:
        explicit RmlSystem(std::function<void(const std::string&)> warn);
        double GetElapsedTime() override;
        bool LogMessage(Rml::Log::Type type, const Rml::String& message) override;
        // The bar's own image names (hu:...) stay as they are; anything else joins the document's path as usual.
        void JoinPath(Rml::String& translated, const Rml::String& documentPath, const Rml::String& path) override;
        void SetClipboardText(const Rml::String& text) override;
        void GetClipboardText(Rml::String& text) override;

    private:
        std::function<void(const std::string&)> m_Warn;
        std::chrono::steady_clock::time_point m_Start;
    };
}
