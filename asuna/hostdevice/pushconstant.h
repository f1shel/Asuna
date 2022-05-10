#ifndef PUSHCONSTANT_H
#define PUSHCONSTANT_H

#include "binding.h"

struct GPUPushConstantGraphics
{
    uint placeholder;
};

struct GPUPushConstantRaytrace
{
    int curFrame;
    int maxRecursionDepth;
    int spp;
    int emittersNum;
};

// Tonemapper used in post.frag
struct GPUPushConstantPost
{
    float brightness;
    float contrast;
    float saturation;
    float vignette;
    float avgLum;
    float zoom;
    vec2  renderingRatio;
    int   autoExposure;
    float Ywhite;        // Burning white
    float key;           // Log-average luminance
};

#endif