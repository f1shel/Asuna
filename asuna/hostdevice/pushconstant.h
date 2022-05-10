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

#endif