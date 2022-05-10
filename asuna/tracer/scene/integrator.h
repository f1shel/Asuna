#pragma once

#include "camera.h"

#include <nvvk/appbase_vk.hpp>

class Integrator
{
  public:
    Integrator(VkExtent2D size, uint32_t spp, uint32_t maxRecurDepth)
    {
        m_size          = size;
        m_spp           = spp;
        m_maxRecurDepth = maxRecurDepth;
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
    uint32_t getMaxRecurDepth()
    {
        return m_maxRecurDepth;
    }

  private:
    VkExtent2D m_size{0, 0};
    uint32_t   m_spp           = 1;
    uint32_t   m_maxRecurDepth = 2;
};