#pragma once

#include <shared/vertex.h>
#include <context/context.h>
#include <nvvk/raytraceKHR_vk.hpp>
#include "alloc.h"
#include "bounding_box.h"
#include "primitive.h"

#include <map>
#include <string>
#include <vector>

class Mesh {
public:
  Mesh(Primitive& prim);
  Mesh(const std::string& meshPath, bool recomputeNormal = false,
       vec2 uvScale = {1.f, 1.f});
  uint getVerticesNum() { return m_vertices.size(); }
  uint getIndicesNum() { return m_indices.size(); }
  const vector<GpuVertex>& getVertices() { return m_vertices; }
  const vector<uint>& getIndices() { return m_indices; }
  const vec3& getPosMin() { return m_posMin; }
  const vec3& getPosMax() { return m_posMax; }

private:
  vector<GpuVertex> m_vertices{};
  vector<uint> m_indices{};
  vec3 m_posMin{BBOX_MAXF};
  vec3 m_posMax{BBOX_MINF};
};

class MeshAlloc : public GpuAlloc {
public:
  MeshAlloc(ContextAware* pContext, Mesh* pMesh, const VkCommandBuffer& cmdBuf);
  void deinit(ContextAware* pContext);
  VkBuffer getIndicesBuffer() { return m_bIndices.buffer; }
  VkBuffer getVerticesBuffer() { return m_bVertices.buffer; }
  uint getIndicesNum() { return m_numIndices; }
  uint getVerticesNum() { return m_numVertices; }
  const vec3& getPosMin() { return m_posMin; }
  const vec3& getPosMax() { return m_posMax; }

private:
  uint m_numIndices{0};
  uint m_numVertices{0};
  vec3 m_posMin{0, 0, 0};
  vec3 m_posMax{0, 0, 0};
  nvvk::Buffer m_bIndices;   // Device buffer of the indices forming triangles
  nvvk::Buffer m_bVertices;  // Device buffer of all 'Vertex'
};

nvvk::RaytracingBuilderKHR::BlasInput MeshBufferToBlas(VkDevice device,
                                                       MeshAlloc& meshAlloc);