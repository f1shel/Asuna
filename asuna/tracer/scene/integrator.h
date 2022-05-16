#pragma once

#include "camera.h"

#include <vulkan/vulkan_core.h>

class Integrator
{
public:
  Integrator() {}
  Integrator(VkExtent2D size, uint spp, uint maxPathDepth)
  {
    m_size         = size;
    m_spp          = spp;
    m_maxPathDepth = maxPathDepth;
  }
  VkExtent2D getSize() { return m_size; }
  uint       getSpp() { return m_spp; }
  uint       getMaxPathDepth() { return m_maxPathDepth; }

private:
  VkExtent2D m_size{0, 0};
  uint       m_spp{1};
  uint       m_maxPathDepth{2};
};