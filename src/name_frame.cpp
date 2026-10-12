#include "name_frame.h"

#include "argb.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace headsup
{
    namespace
    {
        // Locals, set by the routine before the hook.
        constexpr uint32_t kFrameActor  = 0x04;
        constexpr uint32_t kFrameLength = 0x18;
        constexpr uint32_t kFrameX      = 0x30;
        constexpr uint32_t kFrameY      = 0x34;
        constexpr uint32_t kFrameDepth  = 0x48;
        constexpr uint32_t kFrameScaleX = 0x4C;
        constexpr uint32_t kFrameScaleY = 0x50;
        // The return address, then the arguments: the first is not read.
        constexpr uint32_t kFrameCaller     = 0x6D4;
        constexpr uint32_t kFrameText       = 0x6DC;
        constexpr uint32_t kFrameSize       = 0x6E0;
        constexpr uint32_t kFrameColor      = 0x6E4;
        constexpr uint32_t kFrameShellColor = 0x6E8;

        constexpr uint8_t kLineBreak = '\n';
        // The frame's scale is pixels per game unit, and the game's letters are 10 units tall (12 with a descender).
        constexpr float kLetterUnits = 10.0f;
        // A name's depth is 1 - 0.1 / its yalms from the camera, and the game sizes it at 5.12 / yalms + 0.056 of full
        // size, to the nearest eightieth: 513 of 525 names in draw dumps exactly, the rest a step or two off.
        constexpr float kNearPlane = 0.1f, kSizePerYalm = 5.12f, kSizeFar = 0.056f, kSizeSteps = 80.0f;
        // Names are drawn through a modulate-2x texture stage, so the frame's color is half what shows.
        constexpr uint32_t kNameModulate = 2;

        template <typename T>
        T At(const uint8_t* frame, uint32_t offset)
        {
            T value;
            std::memcpy(&value, frame + offset, sizeof(value));
            return value;
        }
    }

    NameFrame ReadNameFrame(const uint8_t* frame)
    {
        NameFrame read;
        read.caller     = At<uint32_t>(frame, kFrameCaller);
        read.actor      = At<uint32_t>(frame, kFrameActor);
        read.length     = At<uint32_t>(frame, kFrameLength);
        read.x          = At<float>(frame, kFrameX);
        read.y          = At<float>(frame, kFrameY);
        read.depth      = At<float>(frame, kFrameDepth);
        read.scaleX     = At<float>(frame, kFrameScaleX);
        read.scaleY     = At<float>(frame, kFrameScaleY);
        read.text       = At<uint32_t>(frame, kFrameText);
        read.size       = At<float>(frame, kFrameSize);
        read.color      = At<uint32_t>(frame, kFrameColor);
        read.shellColor = At<uint32_t>(frame, kFrameShellColor);
        return read;
    }

    bool FromNameRoutine(const NameFrame& frame, std::span<const uint32_t> callerReturns)
    {
        return std::find(callerReturns.begin(), callerReturns.end(), frame.caller) != callerReturns.end() &&
               frame.length > 0 && frame.length <= kMaxNameBytes;
    }

    bool Placeable(const NameFrame& frame)
    {
        // Not scale <= 0, so a NaN scale fails too.
        return frame.scaleX > 0.0f && frame.scaleY > 0.0f && std::isfinite(frame.x) && std::isfinite(frame.y) &&
               std::isfinite(frame.depth);
    }

    bool SingleLine(const uint8_t* text, uint32_t length)
    {
        return std::find(text, text + length, kLineBreak) == text + length;
    }

    float NameSizeAtDepth(float depth)
    {
        if (!(depth > 0.0f && depth < 1.0f)) return 1.0f;
        const float yalms = kNearPlane / (1.0f - depth);
        return std::min(1.0f, std::round((kSizePerYalm / yalms + kSizeFar) * kSizeSteps) / kSizeSteps);
    }

    DrawnName NameFromFrame(const NameFrame& frame, float toBackBufferX, float toBackBufferY, bool picked)
    {
        DrawnName name;
        const float x = frame.x * toBackBufferX, top = frame.y * toBackBufferY;
        float height = kLetterUnits * frame.scaleY * toBackBufferY;
        if (picked && frame.size > 0.0f) height *= std::min(1.0f, NameSizeAtDepth(frame.depth) / frame.size);
        name.box.Add(x, top);
        name.box.Add(x, top + height);
        name.depth = frame.depth;
        name.color = ShownColor(frame.color, kNameModulate);
        return name;
    }
}
