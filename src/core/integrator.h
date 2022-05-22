#pragma once

#include "camera.h"

#include <vulkan/vulkan_core.h>

class Integrator
{
public:
  Integrator() {}
  Integrator(uint spp, uint maxPathDepth, uint useToneMapping, uint useFaceNormal, uint ignoreEmissive)
  {
    m_spp            = spp;
    m_maxPathDepth   = maxPathDepth;
    m_useFaceNormal  = useFaceNormal;
    m_useToneMapping = useToneMapping;
    m_ignoreEmissive = ignoreEmissive;
  }
  uint getSpp() { return m_spp; }
  void setSpp(int spp) { m_spp = spp; }
  uint getMaxPathDepth() { return m_maxPathDepth; }
  uint getUseToneMapping() { return m_useToneMapping; }
  uint getUseFaceNormal() { return m_useFaceNormal; }
  uint getIgnoreEmissive() { return m_ignoreEmissive; }

private:
  uint m_spp{1};
  uint m_maxPathDepth{2};
  uint m_useFaceNormal{0};
  uint m_useToneMapping{1};
  uint m_ignoreEmissive{0};
};