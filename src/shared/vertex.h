#ifndef VERTEX_H
#define VERTEX_H

#include "binding.h"

struct GpuVertex
{
  vec3 pos;
  vec2 uv;
  vec3 normal;
  vec3 tangent;
};

#endif