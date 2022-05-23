#pragma once

#include "camera.h"

#include <vulkan/vulkan_core.h>

class Integrator
{
public:
  Integrator() {}
  Integrator(uint spp, uint maxPathDepth, uint toneMappingType, uint useFaceNormal, uint ignoreEmissive, vec3 bgColor)
  {
    m_spp             = spp;
    m_maxPathDepth    = maxPathDepth;
    m_useFaceNormal   = useFaceNormal;
    m_toneMappingType = toneMappingType;
    m_ignoreEmissive  = ignoreEmissive;
    m_backgroundColor = bgColor;
  }
  uint getSpp() { return m_spp; }
  void setSpp(int spp) { m_spp = spp; }
  uint getMaxPathDepth() { return m_maxPathDepth; }
  uint getToneMappingType() { return m_toneMappingType; }
  uint getUseFaceNormal() { return m_useFaceNormal; }
  uint getIgnoreEmissive() { return m_ignoreEmissive; }
  vec3 getBackgroundColor() { return m_backgroundColor; }

private:
  uint m_spp{1};
  uint m_maxPathDepth{2};
  uint m_useFaceNormal{0};
  uint m_toneMappingType{ToneMappingTypeNone};
  uint m_ignoreEmissive{0};
  vec3 m_backgroundColor{0.f, 0.f, 0.f};
};