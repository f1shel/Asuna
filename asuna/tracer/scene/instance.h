#pragma once

#include "nvmath/nvmath.h"

#include <cstdint>

class Instance
{
public:
	void init(const nvmath::mat4f& tm, uint32_t meshId) {
		m_transform = tm;
		m_meshIndex = meshId;
	}
	void deinit() {}
public:
	nvmath::mat4f m_transform{nvmath::mat4f_id};
	uint32_t      m_meshIndex{ 0 };  // Model index reference
};