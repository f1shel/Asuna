#ifndef PUSHCONSTANT_H
#define PUSHCONSTANT_H

#include "binding.h"

struct GpuPushConstantGraphics
{
  uint placeholder;
};

struct GpuPushConstantRaytrace
{
  int spp;
  int curFrame;
  int maxPathDepth;
  int numLights;
};

// Tonemapper used in post.frag
struct GpuPushConstantPost
{
  float brightness;
  float contrast;
  float saturation;
  float vignette;
  float avgLum;
  float zoom;
  vec2  renderingRatio;
  int   autoExposure;
  float Ywhite;  // Burning white
  float key;     // Log-average luminance
  uint  useTonemapping;
};

#endif