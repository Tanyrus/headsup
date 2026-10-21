#pragma once

#include "Ashita.h"
#include "frame_cache.h"
#include "game_font.h"
#include "icons.h"
#include "plate_image.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

namespace headsup
{
    // One textured, tinted rectangle in render-target pixels. The texture's columns from u0 to u1 and its rows from the
    // top to v are stretched across it.
    struct GpuQuad
    {
        IDirect3DTexture8* texture;
        float x, y, width, height, u0, u1, v, depth;
        uint32_t tint;
    };

    // An A8R8G8B8 managed texture holding the image in its top-left corner, padded to powers of two; u and v are the
    // image's share of it. Null when Direct3D refuses.
    IDirect3DTexture8* CreateTexture(IDirect3DDevice8* device, const void* bgra, int width, int height, float& u,
        float& v);
    void ReleaseTexture(IDirect3DTexture8*& texture);

    // Text drawn and measured with GDI, through one memory device context and the fonts it has made: the context until
    // Release, each font until Sweep finds it idle.
    class GdiText
    {
    public:
        // Text in a font family and its styles (FontStyle bits), white on clear, tracking pixels after each character
        // and margin pixels around it; false when GDI could not draw or measure it.
        bool Rasterize(const char* text, const char* family, int pixelHeight, uint8_t style, int tracking, int margin,
            Coverage& out);
        // The box Rasterize draws the text's letters in, set solid and before its margin, without drawing them; false
        // when GDI cannot measure it.
        bool Extent(const char* text, const char* family, int pixelHeight, uint8_t style, int& width, int& height);
        // The font's underline in the line Rasterize draws (without its margin): the row it starts on and how many
        // rows thick it is. False for a font that does not say, such as a bitmap font.
        bool UnderlineBand(const char* family, int pixelHeight, uint8_t style, int& top, int& thickness);
        // Where the font's ink sits in the line Rasterize draws (without its margin): the top and bottom of the ink of
        // glyphs, and the bottom of an H's. Drawn once for each family, size and glyphs, then kept, past Release too.
        LineInk FontInk(const char* family, int pixelHeight, const char* glyphs);
        // Once a frame: deletes the fonts no text has used for kIdleFrames frames, as a player who tries fonts and
        // sizes would otherwise keep every one.
        void Sweep();
        void Release();
        // The game's own font, which kGameFontName and kGameFontNarrowName draw from; without it they draw in the
        // default font. Release keeps it.
        void SetGameFont(std::optional<GameFontFace> face) { m_GameFont = std::move(face); }

    private:
        // The device context with the font selected and tracking pixels after each character, or null when GDI made
        // neither.
        HDC WithFont(std::string_view family, int pixelHeight, uint8_t style, int tracking);

        std::optional<GameFontFace> m_GameFont;
        HDC m_Context = nullptr;
        FrameCache<std::tuple<std::string, int, uint8_t>, HFONT> m_Fonts;
        std::map<std::tuple<std::string, int, std::string>, LineInk, std::less<>> m_Inks;
        uint32_t m_Frame = 0;
    };

    // An icon's texture, the icon in its top-left corner.
    struct IconTexture
    {
        IDirect3DTexture8* texture = nullptr;
        float width = 0.0f, height = 0.0f, u = 0.0f, v = 0.0f;
    };

    // The icons' textures, each made the first time it is asked for. Once Direct3D refuses one, no more are made.
    class IconTextures
    {
    public:
        // Null once Direct3D could not make one. An icon painted in a color (the linkshell's pearl) has a texture of its
        // own for each color.
        const IconTexture* For(IDirect3DDevice8* device, Icon icon, std::optional<uint32_t> paint);
        bool Failed() const { return m_Failed; }
        // Which icon Direct3D could not make, once.
        std::string TakeFailure();
        void Release();

    private:
        std::array<IconTexture, kIconCount> m_Textures{};
        std::map<std::pair<Icon, uint32_t>, IconTexture> m_Painted; // by icon and color, alpha left out
        bool m_Failed = false;
        std::string m_Failure;
    };

    // Sets every state the quads need; DrawQuad after it, then SetTexture(0, nullptr) when done.
    void SetQuadStates(IDirect3DDevice8* device, bool depthTest);
    void DrawQuad(IDirect3DDevice8* device, const GpuQuad& quad, bool depthTest);
    // A state block of the states SetQuadStates sets, recorded from it: the vertex shader and the render and texture
    // stage states. CaptureStateBlock before the quads and ApplyStateBlock after them put back the game's values of
    // those. It leaves out texture 0 and stream 0 (which DrawPrimitiveUP clears): a block keeps what it captured
    // alive until the next capture, and a default-pool texture or buffer kept alive makes the game's Reset fail.
    // Nothing when Direct3D refuses.
    std::optional<DWORD> RecordQuadStates(IDirect3DDevice8* device);
}
