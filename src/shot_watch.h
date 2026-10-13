#pragma once

namespace headsup
{
    // How many frames in a row Ashita's fonts may be hidden and still be a screenshot's doing: its plugin hides them
    // for the one frame it captures.
    constexpr int kShotFrames = 3;

    // Whether the frame being drawn is one a clean screenshot is taken of: Ashita's screenshot plugin, asked to hide
    // (/screenshot hide), hides Ashita's fonts from that frame's start and shows them again at its Present.
    class ShotWatch
    {
    public:
        // At any point of the frame, with whether Ashita's fonts are visible there. Returns Hiding.
        bool Seen(bool fontsVisible);
        bool Hiding() const;
        void EndFrame();

    private:
        bool m_Hidden = false; // seen hidden in this frame
        int m_Before  = 0;     // frames in a row before this one they were seen hidden in
    };

    inline bool ShotWatch::Seen(bool fontsVisible)
    {
        if (!fontsVisible) m_Hidden = true;
        return Hiding();
    }

    inline bool ShotWatch::Hiding() const { return m_Hidden && m_Before < kShotFrames; }

    inline void ShotWatch::EndFrame()
    {
        m_Before = m_Hidden ? m_Before + 1 : 0;
        m_Hidden = false;
    }
}
