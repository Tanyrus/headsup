#include "rml_host.h"

#include "fonts.h"
#include "utf16.h"

#include <RmlUi/Core/Elements/ElementFormControlInput.h>
#include <RmlUi/Core/Elements/ElementFormControlTextArea.h>
#include <RmlUi/Debugger.h>

#include <utility>

namespace headsup
{
    namespace
    {
        constexpr const char* kBarContext     = "bar";
        constexpr const char* kOverlayContext = "overlay";
        constexpr int kLeftButton = 0, kRightButton = 1, kMiddleButton = 2; // as RmlUi numbers them
        constexpr int kKeyToggledBit      = 0x0001;                          // GetKeyState: a lock is on
        constexpr int kTypedAtMost        = 4;                               // UTF-16 units one key can type
        constexpr wchar_t kFirstPrintable = L' ';
        constexpr wchar_t kDelete         = 0x7F;
        constexpr int kProbeHeight        = 16;         // any size: the font is made only to read its file
        constexpr DWORD kCollectionTable  = 0x66637474; // 'ttcf': GetFontData's name for a whole collection file
        constexpr wchar_t kFirstSurrogate = 0xD800, kLastSurrogate = 0xDFFF; // halves of a pair: no character alone

        bool KeyHeld(int virtualKey) { return (GetKeyState(virtualKey) & kKeyDownBit) != 0; }
        bool LockOn(int virtualKey) { return (GetKeyState(virtualKey) & kKeyToggledBit) != 0; }

        int Modifiers()
        {
            return ModifiersOf(KeyHeld(VK_CONTROL), KeyHeld(VK_SHIFT), KeyHeld(VK_MENU), LockOn(VK_CAPITAL),
                LockOn(VK_NUMLOCK));
        }
    }

    bool RmlHost::Start(IDirect3DDevice8* device, int width, int height, std::function<void(const std::string&)> warn,
        RmlRender::ImageSource images)
    {
        if (Running()) return true;
        m_System.emplace(std::move(warn));
        m_Render.SetDevice(device);
        m_Render.SetImageSource(std::move(images));
        Rml::SetSystemInterface(&*m_System);
        Rml::SetRenderInterface(&m_Render);
        if (!Rml::Initialise()) return false;
        size_t count = 0;
        // A character a font lacks comes from the fallback one (IsFallbackFont).
        for (const BundledFont* font = BundledFonts(count); count-- > 0; ++font)
            m_Fonts[Rml::StringUtilities::ToLower(font->family)] =
                Rml::LoadFontFace(Rml::Span<const Rml::byte>(font->data, font->bytes), font->family,
                    Rml::Style::FontStyle::Normal, Rml::Style::FontWeight::Auto, IsFallbackFont(font->family));
        m_Width   = width;
        m_Height  = height;
        m_Bar     = Rml::CreateContext(kBarContext, Rml::Vector2i(width, height));
        m_Overlay = Rml::CreateContext(kOverlayContext, Rml::Vector2i(width, height));
        if (m_Bar == nullptr || m_Overlay == nullptr)
        {
            Stop();
            return false;
        }
        Rml::Debugger::Initialise(m_Overlay);
        return true;
    }

    void RmlHost::Stop()
    {
        if (m_Overlay != nullptr) Rml::Debugger::Shutdown();
        Rml::Shutdown();
        m_Render.ReleaseLayers();
        m_Bar     = nullptr;
        m_Overlay = nullptr;
        m_System.reset();
        m_Fonts.clear();
        m_FontData.clear();
    }

    bool RmlHost::EnsureFont(const std::string& family)
    {
        if (!Running() || family.empty()) return false;
        const std::string key = Rml::StringUtilities::ToLower(family);
        if (const auto known = m_Fonts.find(key); known != m_Fonts.end()) return known->second;
        bool loaded = false;
        if (HDC dc = CreateCompatibleDC(nullptr); dc != nullptr)
        {
            const std::wstring wide = Utf16(family);
            HFONT font = CreateFontW(-kProbeHeight, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_TT_PRECIS,
                CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, wide.c_str());
            if (font != nullptr)
            {
                const HGDIOBJ was = SelectObject(dc, font);
                // GDI gives another font for a family it lacks: only the one asked for counts.
                wchar_t face[LF_FACESIZE] = {};
                GetTextFaceW(dc, LF_FACESIZE, face);
                if (lstrcmpiW(face, wide.c_str()) == 0)
                {
                    // A collection's whole file when the font is in one, the font's own file otherwise.
                    DWORD table = kCollectionTable;
                    DWORD size  = GetFontData(dc, table, 0, nullptr, 0);
                    if (size == GDI_ERROR)
                    {
                        table = 0;
                        size  = GetFontData(dc, table, 0, nullptr, 0);
                    }
                    if (size != GDI_ERROR && size > 0)
                    {
                        std::vector<Rml::byte>& bytes = m_FontData.emplace_back(size);
                        loaded = GetFontData(dc, table, 0, bytes.data(), size) == size &&
                                 Rml::LoadFontFace(Rml::Span<const Rml::byte>(bytes.data(), bytes.size()), family,
                                     Rml::Style::FontStyle::Normal);
                        if (!loaded) m_FontData.pop_back();
                    }
                }
                SelectObject(dc, was);
                DeleteObject(font);
            }
            DeleteDC(dc);
        }
        m_Fonts[key] = loaded;
        return loaded;
    }

    bool RmlHost::EnsureFontFile(const std::string& name, const std::string& path)
    {
        if (!Running() || name.empty()) return false;
        const std::string key = Rml::StringUtilities::ToLower(name);
        if (const auto known = m_Fonts.find(key); known != m_Fonts.end()) return known->second;
        const bool loaded = GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES &&
                            Rml::LoadFontFace(path, name, Rml::Style::FontStyle::Normal);
        m_Fonts[key] = loaded;
        return loaded;
    }

    float RmlHost::TextWidth(const std::string& text, const std::string& family, int pixels, float tracking)
    {
        if (!Running() || text.empty()) return 0.0f;
        Rml::FontEngineInterface* engine = Rml::GetFontEngineInterface();
        const Rml::FontFaceHandle face   = engine->GetFontFaceHandle(Rml::StringUtilities::ToLower(family),
            Rml::Style::FontStyle::Normal, Rml::Style::FontWeight::Normal, pixels);
        if (face == 0) return 0.0f;
        static const Rml::String kNoLanguage;
        const Rml::TextShapingContext shaping{kNoLanguage, Rml::Style::Direction::Auto, Rml::Style::FontKerning::Auto, tracking};
        return static_cast<float>(engine->GetStringWidth(face, text, shaping));
    }

    Rml::FontMetrics RmlHost::Metrics(const std::string& family, int pixels)
    {
        if (!Running()) return Rml::FontMetrics{};
        Rml::FontEngineInterface* engine = Rml::GetFontEngineInterface();
        const Rml::FontFaceHandle face   = engine->GetFontFaceHandle(Rml::StringUtilities::ToLower(family),
            Rml::Style::FontStyle::Normal, Rml::Style::FontWeight::Normal, pixels);
        return face != 0 ? engine->GetFontMetrics(face) : Rml::FontMetrics{};
    }

    void RmlHost::Resize(int width, int height)
    {
        if (!Running() || (width == m_Width && height == m_Height)) return;
        m_Width  = width;
        m_Height = height;
        m_Bar->SetDimensions(Rml::Vector2i(width, height));
        m_Overlay->SetDimensions(Rml::Vector2i(width, height));
    }

    bool RmlHost::Mouse(uint32_t message, float x, float y, int wheelDelta)
    {
        if (!Running()) return false;
        const int modifiers = Modifiers();
        const int px = static_cast<int>(x), py = static_cast<int>(y);
        // Each returns true when the mouse is not interacting with any element.
        switch (message)
        {
        case WM_MOUSEMOVE: return !m_Overlay->ProcessMouseMove(px, py, modifiers);
        case WM_LBUTTONDOWN: return Press(kLeftButton, modifiers);
        case WM_LBUTTONUP: return !m_Overlay->ProcessMouseButtonUp(kLeftButton, modifiers);
        case WM_RBUTTONDOWN: return Press(kRightButton, modifiers);
        case WM_RBUTTONUP: return !m_Overlay->ProcessMouseButtonUp(kRightButton, modifiers);
        case WM_MBUTTONDOWN: return Press(kMiddleButton, modifiers);
        case WM_MBUTTONUP: return !m_Overlay->ProcessMouseButtonUp(kMiddleButton, modifiers);
        case WM_MOUSEWHEEL: return !m_Overlay->ProcessMouseWheel(WheelNotches(wheelDelta), modifiers);
        default: return false;
        }
    }

    // A press on nothing of HeadsUp's takes the keyboard from its text box or slider, as a click elsewhere in a
    // window does: the box would go on keeping every key from the game, and the slider on moving with the arrows.
    bool RmlHost::Press(int button, int modifiers)
    {
        const bool outside = m_Overlay->ProcessMouseButtonDown(button, modifiers);
        // Its document takes the keyboard: blurred alone, a slider's part would hand it to the slider.
        if (Rml::Element* focus = outside ? m_Overlay->GetFocusElement() : nullptr)
            if (Rml::ElementDocument* document = focus->GetOwnerDocument()) document->Focus();
        return !outside;
    }

    bool RmlHost::Key(uint32_t virtualKey, bool down)
    {
        if (!Running()) return false;
        const bool typing   = WantsText();
        const int modifiers = Modifiers();
        if (down)
        {
            m_Overlay->ProcessKeyDown(KeyOf(virtualKey), modifiers);
            if (typing && (modifiers & (Rml::Input::KM_CTRL | Rml::Input::KM_ALT)) == 0) Type(virtualKey);
        }
        else
            m_Overlay->ProcessKeyUp(KeyOf(virtualKey), modifiers);
        return typing;
    }

    // What the key types with the keyboard as it is: Ashita's hook gives key messages, never the characters.
    void RmlHost::Type(uint32_t virtualKey)
    {
        BYTE state[256];
        if (!GetKeyboardState(state)) return;
        wchar_t typed[kTypedAtMost];
        const int count = ToUnicode(virtualKey, MapVirtualKeyW(virtualKey, MAPVK_VK_TO_VSC), state, typed, kTypedAtMost, 0);
        for (int i = 0; i < count; ++i)
            if (typed[i] >= kFirstPrintable && typed[i] != kDelete && (typed[i] < kFirstSurrogate || typed[i] > kLastSurrogate))
                m_Overlay->ProcessTextInput(static_cast<Rml::Character>(typed[i]));
    }

    bool RmlHost::WantsText() const
    {
        if (!Running()) return false;
        Rml::Element* focus = m_Overlay->GetFocusElement();
        // A window hidden while one of its boxes had the keyboard leaves the context's focus on that box.
        if (focus == nullptr || !focus->IsVisible(true)) return false;
        if (rmlui_dynamic_cast<Rml::ElementFormControlTextArea*>(focus) != nullptr) return true;
        const auto* input = rmlui_dynamic_cast<Rml::ElementFormControlInput*>(focus);
        if (input == nullptr) return false;
        const Rml::String type = input->GetAttribute<Rml::String>("type", "text");
        return type == "text" || type == "password";
    }

    void RmlHost::Render(Rml::Context* context)
    {
        context->Update();
        m_Render.Begin(m_Width, m_Height);
        context->Render();
        m_Render.End();
    }

    void RmlHost::RenderBar()
    {
        if (Running()) Render(m_Bar);
    }

    void RmlHost::RenderOverlay()
    {
        if (Running()) Render(m_Overlay);
    }

    void RmlHost::SetDebugger(bool visible)
    {
        if (Running()) Rml::Debugger::SetVisible(visible);
    }

    bool RmlHost::DebuggerVisible() const { return Running() && Rml::Debugger::IsVisible(); }
}
