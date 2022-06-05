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
struct GpuCamera {
  mat4 rasterToCamera;
  mat4 cameraToWorld;
  mat4 envTransform;
  vec4 fxfycxcy;        // [focal_xy, center_xy], for opencv model
  uint type;            // camera type
  float aperture;       // aspect, for thin len model
  float focalDistance;  // focal distance, for thin len model
  float padding;
};

#endif