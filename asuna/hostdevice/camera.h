#ifndef CAMERA_H
#define CAMERA_H

#include "binding.h"

// Uniform buffer set at each frame
struct Camera
{
    mat4 viewProj;     // Camera view * projection
    mat4 viewInverse;  // Camera inverse view matrix
    mat4 projInverse;  // Camera inverse projection matrix
};

#endif