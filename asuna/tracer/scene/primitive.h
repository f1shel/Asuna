#pragma once

#include "../../hostdevice/binding.h"

enum PrimitiveType
{
  PrimitiveTypeTriangle  = 0,
  PrimitiveTypeRect      = 1,
  PrimitiveTypeSphere    = 2,
  PrimitiveTypeUndefined = 3
};

struct Primitive
{
  vec3          position = vec3(0.0);
  vec3          u        = vec3(0.0);
  vec3          v        = vec3(0.0);
  float         raidus   = 0.0;
  PrimitiveType type     = PrimitiveTypeUndefined;
};