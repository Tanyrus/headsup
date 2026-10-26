#pragma once

#include "bar_document.h"
#include "bar_game_help.h"
#include "bar_layout.h"
#include "bar_look.h"
#include "bar_widgets.h"
#include "gpu.h"
#include "memo.h"
#include "rml_host.h"
#include "settings.h"

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace headsup
{
    // The top bar as an RmlUi document: the widgets' content and the bar's layout are the bar's own (bar_widgets,
    // bar_layout), measured with RmlUi's fonts, and each widget is an element placed where the layout puts it.
    class RmlBar
    {
    public:
        // Each frame: lays out the bar, shows it in the host's bar context and tells input where everything is.
        // While the bar is off it closes its document. hidden: laid out, but not drawn. skin: the game's window art in
        // use, once it is read.
        void Update(RmlHost& host, const BarState& state, const Settings& settings, const GameSkinLook* skin,
            float screenWidth, float screenHeight, BarInput& input, bool hidden);
        // Closes the bar's document; before the host stops, and while another renderer draws the bar.
        void Close();
        void Release();
        // The most Game help's bar can cover while it is on, in back buffer pixels.
        std::optional<BarRect> HelpBarReach() const { return m_HelpReach; }
        // The top bar's bottom, its under line counted, while it draws; 0 while it does not.
        float BarBottomNow() const { return m_BarBottom; }
        // The title size the last frame drew, 0 before one and while the bar is off.
        int TitlePixels() const { return m_TitlePixels; }

    private:
        // The bar's text sizes and places follow its height, its text size setting and its shape and fonts.
        struct LinesKey
        {
            float height   = 0.0f;
            BarShape shape = BarShape::Ornate;
            int barText    = kAutoBarTextSize;
            const char* titleFont  = nullptr; // interned by BarTitleFont and BarTextFont: one pointer per name
            const char* detailFont = nullptr;
            bool operator==(const LinesKey&) const = default;
        };
        struct Rows
        {
            TextRow lines;
            BarTextPlace place;
            float helpBarHeight = 0.0f; // the same whatever it shows
        };
        // What the document was built from: a change builds it again.
        struct DocumentKey
        {
            BarPalette palette{};
            BarMetrics metrics{};
            BarRow row{};
            TextRow lines{};
            BarTextPlace place{};
            const char* titleFont  = nullptr;
            const char* detailFont = nullptr;
            std::vector<WidgetText> own;
            std::optional<SkinFrame> skin;
            bool operator==(const DocumentKey&) const = default;
        };
        // A value RmlUi was last given, so it is given only when it changes.
        struct Px
        {
            std::optional<float> value;
        };
        // A widget's element and what it shows now.
        struct Shown
        {
            Rml::Element* element = nullptr;
            WidgetContent content; // as the widget gave it, which its natural width was measured from
            float natural = 0.0f;
            float padded  = 0.0f; // the room from its edges to what it shows, that natural was measured with
            bool measured = false;
            WidgetContent drawn; // as its markup was made from: the target's is cut to its room
            bool marked = false; // its markup is drawn's
            Px left, width, padding;
        };
        struct Divider
        {
            Rml::Element* element = nullptr;
            Px left;
        };
        struct FitKey
        {
            std::string text;
            std::string_view font;
            int pixels = 0;
            long room  = 0; // in tenths of a pixel
            bool operator==(const FitKey&) const = default;
        };
        struct FaceDigit
        {
            std::string_view family;
            int pixels = 0;
            char digit = '0';
        };
        struct Widgets
        {
            std::array<WidgetContent, kPanelSlots> contents;
            std::array<bool, kPanelSlots> hasPopup{};
            std::array<float, kPanelSlots> spacing{}; // each widget's own room more beside it
            std::array<std::optional<WidgetText>, kPanelSlots> text; // a widget's own, where it has fonts or a size of its own
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
        // sizes and fonts, and the room below the bar, which cuts it.
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
        struct PopupDocumentKey
        {
            BarPalette palette{};
            BarMetrics metrics{};
            const char* titleFont  = nullptr;
            const char* detailFont = nullptr;
            std::optional<SkinFrame> skin;
            bool operator==(const PopupDocumentKey&) const = default;
        };
        // A font's line as GDI draws it, which the popups are laid out by (its height, and its ink's top and bottom
        // in it), and as RmlUi sets it: its line box's height, and how far under GDI's line top that box starts for
        // the two baselines to meet.
        struct FaceLine
        {
            std::string_view family;
            int pixels    = 0;
            float height  = 0.0f, inkTop = 0.0f, inkBottom = 0.0f;
            int rmlHeight = 0;
            float drop    = 0.0f;
        };

        const Rows& RowsFor(RmlHost& host, const BarMetrics& metrics, BarShape shape, int barText, const char* titleFont,
            const char* detailFont);
        Rows MakeRows(RmlHost& host, const BarMetrics& metrics, const char* titleFont, const char* detailFont);
        // The same for a widget's own text, kept for each one asked for while it is asked for.
        const Rows& WidgetRowsFor(RmlHost& host, const BarMetrics& metrics, BarShape shape, int barText, const char* titleFont,
            const char* detailFont);
        // The widgets with text of their own, as the document's rules and their content take it.
        void OwnText(RmlHost& host, const BarLook& look, const Settings& settings, Widgets& widgets);
        // The look a widget is measured and written in: the bar's, with its own text where it has some.
        static BarLook LookOf(const BarLook& look, const Widgets& widgets, int slot);
        BarLinePlace PlaceLine(RmlHost& host, const char* family, int pixels, const char* glyphs, float top, float aloneTop);
        // Builds the document again when what it is made from changed. False when RmlUi refused it.
        bool Document(RmlHost& host, const BarLook& look, const BarTextPlace& place, const Widgets& widgets);
        BarLayout PlaceWidgets(RmlHost& host, const BarLook& look, const BarState& state, float screenWidth,
            Widgets& widgets);
        void ShowWidgets(const BarLook& look, const BarLayout& layout, const Widgets& widgets, const BarInput& input,
            float screenWidth);
        // Game help's own bar under the top bar, from top and moved offsetX: where its box went (empty for none).
        // cut: its text was, so the bar has a popup with all of it.
        BarRect ShowHelpBar(RmlHost& host, const BarLook& look, const Rows& rows, const BarState& state, float top,
            float offsetX, float screenWidth, bool& cut);
        // The open popup in the host's overlay, and where it went (an empty rectangle for none).
        BarRect ShowPopup(RmlHost& host, const BarLook& look, const BarState& state,
            const BarLayout& layout, float screenWidth, float screenHeight, const BarInput& input, bool hidden);
        bool PopupDocumentFor(RmlHost& host, const BarLook& look);
        void ClosePopup();
        const FaceLine& FaceLineOf(RmlHost& host, const char* family, int pixels);

        float Width(RmlHost& host, const std::string& text, const TextFace& face) const;
        float SpansWidth(RmlHost& host, const std::vector<Span>& spans, const TextFace& face) const;
        float LineWidth(RmlHost& host, const std::vector<Span>& spans, const std::vector<Icon>& icons,
            const TextFace& face) const;
        // The room a widget's line takes: its width with every figure as wide as the font's widest, so a clock or a
        // counter ticking does not move the widgets beside it.
        float SteadyWidth(RmlHost& host, std::vector<Span> spans, const std::vector<Icon>& icons, const TextFace& face);
        char WidestDigitOf(RmlHost& host, const TextFace& face);
        const std::string& Fitted(RmlHost& host, Memo<FitKey, std::string>& memo, std::string text, const TextFace& face,
            float room);

        GdiText m_Gdi; // the bar's text rows are sized by GDI's ink, as the bar drawn with it is
        Memo<LinesKey, Rows> m_Rows;
        std::vector<std::pair<LinesKey, Rows>> m_WidgetRows, m_WidgetRowsBefore; // this frame's, and the last frame's
        Rml::ElementDocument* m_Document = nullptr;
        DocumentKey m_DocumentKey;
        bool m_Visible = false;
        std::array<Shown, kPanelSlots> m_Shown;
        std::array<Divider, kDividerCount> m_Dividers;
        Rml::Element* m_RightCap = nullptr;
        Px m_RightCapLeft;
        Rml::Element* m_Help = nullptr;
        std::string m_HelpRml;
        Memo<HelpBarKey, HelpBarText> m_HelpBar;
        Rml::ElementDocument* m_PopupDocument = nullptr;
        PopupDocumentKey m_PopupDocumentKey;
        Rml::Element* m_Popup = nullptr;
        bool m_PopupMarked    = false; // its markup is the layout's
        Px m_PopupLeft, m_PopupTop, m_PopupWidth, m_PopupHeight;
        Memo<PopupKey, PopupLayout> m_PopupLayout;
        PopupFaces m_PopupFaces;
        std::vector<FaceLine> m_FaceLines;
        Memo<FitKey, std::string> m_TitleFit, m_SubtitleFit;
        std::vector<FaceDigit> m_WidestDigits;
        std::optional<BarRect> m_HelpReach;
        float m_BarBottom = 0.0f;
        int m_TitlePixels = 0;
    };
}
