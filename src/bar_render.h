#pragma once

#include "Ashita.h"
#include "argb.h"
#include "bar_game_help.h"
#include "bar_layout.h"
#include "bar_look.h"
#include "bar_widgets.h"
#include "frame_cache.h"
#include "gpu.h"
#include "memo.h"
#include "popup_layout.h"
#include "settings.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace headsup
{
    class BarRenderer
    {
    public:
        void SetDevice(IDirect3DDevice8* device) { m_Device = device; }
        // Lays out this frame's bar and open popup, makes their quads and tells input where everything is. While the
        // bar is off or failed it releases everything it made, so call it every frame. hidden: laid out, so windows
        // under it keep their places, but drawn and clicked as nothing.
        void Update(const BarState& state, const Settings& settings, float screenWidth, float screenHeight,
            BarInput& input, bool hidden);
        // The bar on the back buffer, after the nameplates. Changes render states.
        void Draw();
        // The open popup, as an ImGui window in its tooltip layer: Ashita draws ImGui after every plugin has drawn, and
        // that layer above every other window, other addons' too. Call it each frame from Present, after Update; scale
        // is ImGui's pixels for each of the back buffer's.
        void DrawPopup(IGuiManager* gui, float scale) const;
        // The open popup as quads, with the quad states set: from the device's Present, over everything ImGui drew.
        void DrawPopupQuads();
        void Release();
        bool Failed() const { return m_Failed; }
        // The most Game help's bar can cover, its caption and every line, while it is on: what game windows are moved
        // out from under. In back buffer pixels.
        std::optional<BarRect> HelpBarReach() const { return m_HelpReach; }
        // The top bar's bottom, its under line counted, while it draws; 0 while it does not.
        float BarBottomNow() const { return m_BarBottom; }
        // The title size the last frame drew, 0 before one and while the bar is off.
        int TitlePixels() const { return m_TitlePixels; }
        std::string TakeFailure();

    private:
        struct Texture
        {
            IDirect3DTexture8* texture = nullptr;
            float width = 0.0f, height = 0.0f, u = 0.0f, v = 0.0f;
            float halo = 0.0f; // the margin around what was drawn, in texture pixels
            InkBox ink;        // text: where its glyphs' ink is in the image
            LineInk line;      // text: where its font's ink is in its line (FontInk)
            std::vector<SliceSpan> spans; // a box (SliceBox), whose width is the whole box's: how it is drawn
        };
        struct TextKey
        {
            std::string text;
            std::string_view font; // a theme's font or BarTitleFont's or BarTextFont's: kept by reference
            int height   = 0;
            int tracking = 0;
            auto operator<=>(const TextKey&) const = default;
        };
        // A text's key as its caller has it: looked up without copying the text.
        struct TextView
        {
            std::string_view text, font;
            int height   = 0;
            int tracking = 0;
            explicit operator TextKey() const { return TextKey{std::string(text), font, height, tracking}; }
            friend auto operator<=>(const TextView& view, const TextKey& key)
            {
                return std::tie(view.text, view.font, view.height, view.tracking) <=>
                       std::tie(key.text, key.font, key.height, key.tracking);
            }
        };
        // Every image the bar draws besides text and icons, by what made it.
        struct ImageKey
        {
            int kind = 0, width = 0, height = 0;
            int radius   = 0; // a box's corners, in hundredths of a pixel
            int outerGap = 0; // a box's
            int below    = 0; // the strip's rows fading under it
            uint32_t colors[6] = {};
            auto operator<=>(const ImageKey&) const = default;
        };

        // What the target's text is cut by FitText from: measuring it takes GDI a call for each cut it tries.
        struct FitKey
        {
            std::string text;
            std::string_view font;
            int pixels = 0;
            long room  = 0; // in tenths of a pixel
            bool operator==(const FitKey&) const = default;
        };
        // The least of the target's name the bar keeps (Shortest), to be measured.
        struct ShortestKey
        {
            std::string text;
            std::string_view font;
            int pixels = 0;
            bool operator==(const ShortestKey&) const = default;
        };
        // The help bar's text follows the game's, its fonts and their sizes, and the room its lines and its caption
        // have beside the bag's count.
        struct HelpBarKey
        {
            StripText strip;
            std::string_view captionFont, lineFont;
            int captionPixels = 0, linePixels = 0;
            long room = 0, gap = 0; // in tenths of a pixel
            bool operator==(const HelpBarKey&) const = default;
        };
        // A popup's layout follows its content, its pin, the bar's height and shape and its text size, which pick its
        // sizes and fonts, and the room below the bar, which cuts it; the content changes once a second at most.
        struct PopupKey
        {
            PopupContent content;
            bool pinned     = false;
            float height    = 0.0f;
            float popupText = 0.0f;
            BarShape shape  = BarShape::Ornate;
            float room      = 0.0f;
            bool operator==(const PopupKey&) const = default;
        };
        // The bar's text sizes and places follow its height, its text size setting and its shape, which picks the
        // fonts.
        struct LinesKey
        {
            float height   = 0.0f;
            BarShape shape = BarShape::Ornate;
            int barText    = kAutoBarTextSize;
            const char* titleFont  = nullptr; // interned by BarTitleFont and BarTextFont: one pointer per name
            const char* detailFont = nullptr;
            bool operator==(const LinesKey&) const = default;
        };

        struct Widgets
        {
            std::array<WidgetContent, kBarWidgetCount> contents;
            std::array<bool, kBarWidgetCount> hasPopup{};
        };

        // Each widget's content and the bar's layout, the target's and Game help's text cut to their room; tells input
        // where they are.
        BarLayout PlaceWidgets(const BarLook& look, const BarState& state, float screenWidth, Widgets& widgets);
        void AddProgress(const BarLook& look, const BarRect& widget, float done, bool beside);
        // Game help's own bar under the top bar, from top near the screen's left edge: the game's caption and its help
        // on one row in a box bordered all round, while it has either. cut: its text was, so the bar has a popup with
        // all of it.
        BarRect AddHelpBar(const BarLook& look, const BarState& state, float top, float screenWidth, bool& cut);
        // The help bar's height, the same whatever it shows.
        float HelpBarHeight(const BarLook& look);
        // The strip across the screen, and the ornate end caps and dividers.
        void AddStrip(const BarLook& look, const BarLayout& layout, float screenWidth);
        // Each widget's pill or frame, icons, coin and text.
        void AddWidgets(const BarLook& look, const BarLayout& layout, const Widgets& widgets, const BarInput& input);
        // The open popup, and where it went (an empty rectangle for none).
        BarRect AddPopup(const BarLook& look, const BarState& state, const BarLayout& layout, float screenWidth,
            float screenHeight, const BarInput& input);

        // White text, colored by the tint it is drawn with; its Texture says where its glyphs' ink is.
        const Texture* Text(const std::string& text, const TextFace& face);
        // A text's line box without its texture's margin, where its font's ink is in it, and its glyphs' overhang.
        static TextSize MeasuredSize(const Texture* text);
        float SpansWidth(const std::vector<Span>& spans, const TextFace& face);
        // A line's text and the icons after it.
        float LineWidth(const std::vector<Span>& spans, const std::vector<Icon>& icons, const TextFace& face);
        const TextRow& Lines(const BarMetrics& metrics, BarShape shape, int barText, const char* titleFont,
            const char* detailFont);
        const std::string& Fitted(Memo<FitKey, std::string>& memo, std::string text, const TextFace& face, float room);
        float ShortestWidth(Memo<ShortestKey, float>& memo, const std::string& text, const TextFace& face);
        // The image make() returns, uploaded when the key has none yet.
        template <typename Make>
        const Texture* Picture(const ImageKey& key, const Make& make);
        // A box's texture, rasterized at width and height rounded to whole pixels.
        const Texture* BoxPicture(float width, float height, const BoxStyle& style);

        bool Upload(Texture& t, const Image& image, float halo = 0.0f);
        bool Upload(Texture& t, const SlicedBox& box);
        // A texture's drawn part at x, y, at its own size (text) or stretched to width by height (images).
        void AddText(const Texture* t, float x, float y, uint32_t tint = kWhite);
        void AddImage(const Texture* t, float x, float y, float width, float height, uint32_t tint = kWhite);
        void AddIcon(Icon icon, float x, float y, float width, float height, uint32_t tint = kWhite);
        // A box at its own size, its middle column stretched between its sides: drawn stretched, its right and
        // bottom border would blur.
        void AddBox(const Texture* t, float x, float y, uint32_t tint = kWhite);
        void Fail(std::string what);

        IDirect3DDevice8* m_Device = nullptr;
        GdiText m_Gdi;
        FrameCache<TextKey, Texture> m_Texts;
        FrameCache<ImageKey, Texture> m_Images;
        IconTextures m_Icons;
        std::vector<GpuQuad> m_Quads;
        size_t m_PopupStart = 0; // the bar's quads come before it in m_Quads, the open popup's from it
        // Apart, or one evicts the other each frame.
        Memo<FitKey, std::string> m_TitleFit, m_SubtitleFit;
        Memo<ShortestKey, float> m_Shortest;
        Memo<HelpBarKey, HelpBarText> m_HelpBar;
        Memo<LinesKey, TextRow> m_Lines;
        Memo<PopupKey, PopupLayout> m_Popup;
        uint32_t m_Frame  = 0;
        int m_TitlePixels = 0;
        std::optional<BarRect> m_HelpReach;
        float m_BarBottom = 0.0f;
        bool m_Failed     = false;
        std::string m_Failure;
    };
}
