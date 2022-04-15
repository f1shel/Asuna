#pragma once

#include "../../hostdevice/scene.h"
#include "../../hostdevice/vertex.h"
#include "../context/context.h"

#include <map>
#include <string>
#include <vector>

class Mesh
{
  public:
	void init(const std::string &meshPath);
	void deinit()
	{
		m_vertices.clear();
		m_indices.clear();
	}

  public:
	std::vector<Vertex>   m_vertices{};
	std::vector<uint32_t> m_indices{};
};

class MeshAlloc
{
  public:
	void init(ContextAware *pContext, Mesh *pMesh);
	void init(ContextAware *pContext, Mesh *pMesh, const VkCommandBuffer &cmdBuf);
	void deinit(ContextAware *pContext);

  public:
	uint32_t     m_nIndices{0};
	uint32_t     m_nVertices{0};
	nvvk::Buffer m_bIndices;         // Device buffer of the indices forming triangles
	nvvk::Buffer m_bVertices;        // Device buffer of all 'Vertex'
};

class SceneDescAlloc
{
  public:
	void init(ContextAware *pContext, const std::map<uint32_t, MeshAlloc *> &meshAllocLUT, const VkCommandBuffer &cmdBuf);
	void deinit(ContextAware *pContext);

  public:
	std::vector<MeshDesc> m_sceneDesc{};
	nvvk::Buffer          m_bSceneDesc;
};