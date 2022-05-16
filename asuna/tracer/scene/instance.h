#pragma once

#include <cstdint>
#include <vector>
#include "nvmath/nvmath.h"
#include "../context/context.h"
#include "alloc.h"
#include "../../hostdevice/binding.h"
#include "../../hostdevice/instance.h"
#include "mesh.h"
#include "material.h"

class Instance
{
public:
  Instance(const nvmath::mat4f& tm, uint meshId, uint matId)
  {
    m_transform     = tm;
    m_meshIndex     = meshId;
    m_materialIndex = matId;
    m_lightIndex    = -1;
  }
  Instance(uint meshId, uint lightId)
  {
    m_meshIndex  = meshId;
    m_lightIndex = lightId;
  }
  const mat4& getTransform() { return m_transform; }
  uint        getMeshIndex() { return m_meshIndex; }
  uint        getMaterialIndex() { return m_materialIndex; }
  int         getLightIndex() { return m_lightIndex; }

private:
  mat4 m_transform{nvmath::mat4f_id};
  uint m_meshIndex{0};      // Model index reference
  uint m_materialIndex{0};  // Material index reference
  int  m_lightIndex{-1};
};

class InstancesAlloc : public GpuAlloc
{
public:
  InstancesAlloc(ContextAware*           pContext,
                 vector<Instance>&       instances,
                 vector<MeshAlloc*>&     meshAllocs,
                 vector<MaterialAlloc*>& materialAllocs,
                 const VkCommandBuffer&  cmdBuf);
  void     deinit(ContextAware* pContext);
  VkBuffer getBuffer() { return m_bInstances.buffer; }

private:
  vector<GpuInstance> m_instances{};
  nvvk::Buffer        m_bInstances;
};