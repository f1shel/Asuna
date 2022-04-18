#pragma once

#include "../../hostdevice/scene.h"
#include "../../hostdevice/vertex.h"
#include "../context/context.h"
#include "utils.h"

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
	std::vector<GPUVertex> m_vertices{};
	std::vector<uint32_t>  m_indices{};

  private:
	nvmath::vec3f m_posMin{BBOX_MAXF, BBOX_MAXF, BBOX_MAXF};
	nvmath::vec3f m_posMax{BBOX_MINF, BBOX_MINF, BBOX_MINF};

  public:
	nvmath::vec3f getPosMin()
	{
		return m_posMin;
	}
	nvmath::vec3f getPosMax()
	{
		return m_posMax;
	}
};

class MeshAlloc
{
  public:
	void init(ContextAware *pContext, Mesh *pMesh);
	void init(ContextAware *pContext, Mesh *pMesh, const VkCommandBuffer &cmdBuf);
	void deinit(ContextAware *pContext);

  public:
	uint32_t      m_nIndices{0};
	uint32_t      m_nVertices{0};
	nvvk::Buffer  m_bIndices;         // Device buffer of the indices forming triangles
	nvvk::Buffer  m_bVertices;        // Device buffer of all 'Vertex'
	nvmath::vec3f m_posMin{0, 0, 0};
	nvmath::vec3f m_posMax{0, 0, 0};
};

class SceneDescAlloc
{
  public:
	void init(ContextAware *pContext, const std::map<uint32_t, MeshAlloc *> &meshAllocLUT,
	          const VkCommandBuffer &cmdBuf);
	void deinit(ContextAware *pContext);

  public:
	std::vector<GPUMeshDesc> m_sceneDesc{};
	nvvk::Buffer             m_bSceneDesc;
};