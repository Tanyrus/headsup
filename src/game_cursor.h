#pragma once

#include "screen_box.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace headsup
{
    // The entities that get HeadsUp's target cursor, by target index (0 for none): the target and, while a sub-target
    // is being picked, the candidate under the sub-target cursor. locked: the player is locked on to the target.
    struct CursorTargets
    {
        uint16_t target    = 0;
        uint16_t subTarget = 0;
        bool locked        = false;
        bool outOfRange    = false; // the candidate being picked is out of range of the spell or ability
        // Where the game puts its arrows over the target and the candidate (its target window's anchors), on screen;
        // known only once the menu's size is.
        bool anchored    = false;
        float anchorX    = 0.0f, anchorY = 0.0f;
        float subAnchorX = 0.0f, subAnchorY = 0.0f;
    };
    // While a sub-target is picked, the game draws its arrow over the candidate red when it is out of range of the spell
    // or ability and blue when it is in range; its arrow over the target stays gray. From an arrow's color: whether
    // the candidate is out of range, or nothing for a gray arrow.
    std::optional<bool> PickedOutOfRange(uint32_t argb);

    // From ITarget's two slots: while a sub-target is being picked, slot 1 holds the target and slot 0 the candidate.
    // An index past the entity map counts as none.
    CursorTargets TargetsFromSlots(bool picking, uint32_t slot0, uint32_t slot1, bool locked, uint32_t entityCount);

    // An entity with HeadsUp's cursor this frame, and the game's name and icons it stands over (back-buffer pixels).
    struct CursorName
    {
        uint16_t index;
        ScreenBox name;
    };

    // Where the game draws a target cursor: its bottom center, in the UI image's pixels, for one entity.
    struct CursorAnchor
    {
        uint16_t index;
        float x, y;
    };
    // The target window's cursor positions (m_AnkX/Y, and m_SubAnkX/Y for the sub-target cursor).
    struct CursorWindow
    {
        float ankX, ankY, subAnkX, subAnkY;
    };
    // The anchors of the entities with HeadsUp's cursor: the main one, and while a sub-target is being picked the sub
    // one too. The sub anchor keeps its last position after picking ends, so it means nothing then.
    std::vector<CursorAnchor> GameCursorAnchors(const std::vector<CursorName>& names, const CursorWindow& window, bool picking);

    // Whether a UI quad (in its pixels) sits on an anchor: centered on it, with its bottom on it or up to the quad's
    // height above, as the cursor bobs.
    bool AtCursorAnchor(const ScreenBox& quad, float anchorX, float anchorY);
    // Whether a quad in the target arrows' texture is one of the arrows: they are taller than wide, and the menu's
    // pointer, drawn from the same texture, is wider.
    bool LooksLikeTargetArrow(const ScreenBox& quad);
    // Whether a quad (back-buffer pixels) is sized like the game's target cursor, about 27x43 on 1440p.
    bool CursorSized(const ScreenBox& quad);
    // Whether a UI quad (back-buffer pixels) is the game's cursor over this name: cursor-sized, centered on it, with its
    // bottom just above the name's top. The game centers it on the name and the icons beside it together.
    bool IsGameCursor(const ScreenBox& quad, const ScreenBox& name);

    // A quad the game draws in its UI layer: in the UI image's pixels and in back-buffer pixels, and its texture.
    struct CursorQuad
    {
        ScreenBox ui;
        ScreenBox screen;
        uintptr_t texture;
    };
    // Whether the quad might be the game's cursor over an entity with HeadsUp's, and so worth finding who drew it.
    bool MayBeGameCursor(const CursorQuad& quad, uintptr_t arrowTexture, const std::vector<CursorAnchor>& anchors,
        const std::vector<CursorName>& names);

    struct CursorVerdict
    {
        bool block      = false;
        bool learnArrow = false; // the quad's texture is the arrows'
    };
    // Whether to block the quad, only ever while HeadsUp draws a cursor: an arrow in the arrows' texture once learned
    // (arrowTexture, 0 before), a quad on an anchor whoever the game credits it to, or one over a name (IsGameCursor)
    // credited to that entity (owner). The arrows' texture is learned only from an arrow credited to its entity, taller
    // than any letter, and never from the names' font (fontTexture), whose letters are credited to the target too.
    CursorVerdict JudgeGameCursor(const CursorQuad& quad, uintptr_t arrowTexture, uintptr_t fontTexture,
        const std::vector<CursorAnchor>& anchors, const std::vector<CursorName>& names, std::optional<uint16_t> owner);
}
