#pragma once

#include <cstdint>
#include <string>

namespace headsup
{
    // The states the game's scene copy draws with, read before HeadsUp draws into the scene and again once the state
    // block has put the game's back. Values as Direct3D keeps them: this file is built without its headers.
    struct CopyState
    {
        uintptr_t texture     = 0; // stage 0's, as an identity
        uint32_t colorOp      = 0, colorArg1 = 0, colorArg2 = 0, alphaOp = 0;
        uint32_t vertexShader = 0;
        uint32_t viewportX = 0, viewportY = 0, viewportWidth = 0, viewportHeight = 0;
        uint32_t zEnable = 0, alphaBlend = 0, srcBlend = 0, destBlend = 0;
    };

    // The names of the states that differ, comma separated; empty when every one came back.
    std::string StatesNotRestored(const CopyState& before, const CopyState& after);

    // Whether pretransformed vertices (each begins with its x and y as floats, stride bytes apart) reach every edge of
    // a width by height target to within a pixel: the scene's copy to the back buffer does, and so does the game's
    // interface laid over it.
    bool CoversTarget(const void* vertices, uint32_t count, uint32_t stride, float width, float height);
}
