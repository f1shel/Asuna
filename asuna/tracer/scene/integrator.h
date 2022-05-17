#pragma once

#include "camera.h"

#include <vulkan/vulkan_core.h>

class Integrator
{
public:
  Integrator() {}
  Integrator(uint spp, uint maxPathDepth)
  {
    m_spp          = spp;
    m_maxPathDepth = maxPathDepth;
  }
  uint       getSpp() { return m_spp; }
  void       setSpp(int spp) { m_spp = spp; }
  uint       getMaxPathDepth() { return m_maxPathDepth; }

private:
  uint       m_spp{1};
  uint       m_maxPathDepth{2};
};