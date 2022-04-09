#pragma once

#include <nvvk/appbase_vk.hpp>

class Sensor
{
public:
	void init() {}
	void deinit() {}
	VkExtent2D m_size{ 0,0 };
};