#pragma once

#include "camera.h"

#include <nvvk/appbase_vk.hpp>

class Integrator
{
  public:
	Integrator(VkExtent2D size, int spp, int maxRecurDepth)
	{
		m_size          = size;
		m_spp           = spp;
		m_maxRecurDepth = maxRecurDepth;
	}
	VkExtent2D getSize()
	{
		return m_size;
	}

  private:
	VkExtent2D m_size{0, 0};
	int        m_spp           = 1;
	int        m_maxRecurDepth = 2;
};