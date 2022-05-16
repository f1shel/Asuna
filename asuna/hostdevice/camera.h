#ifndef CAMERA_H
#define CAMERA_H

#include "binding.h"

// clang-format off
START_ENUM(CameraType)
  CameraTypePerspective = 0,
  CameraTypeOpencv      = 1,
  CameraTypeUndefined   = 2
END_ENUM();
// clang-format on

// Uniform buffer set at each frame
struct GpuCamera
{
  mat4 viewInverse;  // Camera inverse view matrix
  mat4 projInverse;  // Camera inverse projection matrix
  vec4 fxfycxcy;     // [fx, fy, cx, cy]
  uint type;
};

#endif