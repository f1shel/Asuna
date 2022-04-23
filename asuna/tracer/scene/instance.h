#pragma once

#include <cstdint>
#include <vector>
#include "nvmath/nvmath.h"

class Instance
{
  public:
    Instance(const nvmath::mat4f &tm, uint32_t meshId, uint32_t matId)
    {
        m_transform     = tm;
        m_meshIndex     = meshId;
        m_materialIndex = matId;
    }

  public:
    nvmath::mat4f m_transform{nvmath::mat4f_id};
    uint32_t      m_meshIndex{0};            // Model index reference
    uint32_t      m_materialIndex{0};        // Material index reference
};