#ifndef CAMERA_H
#define CAMERA_H

#include "binding.h"

// Uniform buffer set at each frame
struct GPUCamera
{
    mat4 viewInverse;        // Camera inverse view matrix
    mat4 projInverse;        // Camera inverse projection matrix
    vec4 intrinsic;          // [fx, fy, cx, cy]
};

#endif