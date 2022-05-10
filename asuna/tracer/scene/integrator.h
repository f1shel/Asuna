#pragma once

#include "camera.h"

#include <nvvk/appbase_vk.hpp>

class Integrator
{
  public:
    Integrator(VkExtent2D size, uint32_t spp, uint32_t maxPathDepth)
    {
        m_size         = size;
        m_spp          = spp;
        m_maxPathDepth = maxPathDepth;
        CameraManip.setWindowSize(m_size.width, m_size.height);
    }
    VkExtent2D getSize()
    {
        return m_size;
    }
    uint32_t getSpp()
    {
        return m_spp;
    }
    uint32_t getMaxPathDepth()
    {
        return m_maxPathDepth;
    }

  private:
    VkExtent2D m_size{0, 0};
    uint32_t   m_spp          = 1;
    uint32_t   m_maxPathDepth = 2;
};