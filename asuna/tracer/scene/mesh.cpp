#include "mesh.h"

#include <nvh/nvprint.hpp>
#include <nvvk/buffers_vk.hpp>
#include <nvvk/commands_vk.hpp>
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

void Mesh::init(const std::string &meshPath)
{
	tinyobj::ObjReader reader;
	reader.ParseFromFile(meshPath);
	if (!reader.Valid())
	{
		LOGE("[x] Scene Error: load mesh from %s ----> %s\n",
		     meshPath.c_str(), reader.Error().c_str());
		exit(1);
	}
	if (reader.GetShapes().size() != 1)
	{
		LOGE("[x] Scene loader: load mesh from %s ----> asuna tracer supports only one shape per mesh\n",
		     meshPath.c_str());
		exit(1);
	}
	const tinyobj::attrib_t &attrib = reader.GetAttrib();
	const auto              &shape  = reader.GetShapes()[0];

	m_vertices.reserve(shape.mesh.indices.size());
	m_indices.reserve(shape.mesh.indices.size());

	for (const auto &index : shape.mesh.indices)
	{
		Vertex       vertex = {};
		const float *vp     = &attrib.vertices[3 * index.vertex_index];
		vertex.pos          = {*(vp + 0), *(vp + 1), *(vp + 2)};

		if (!attrib.normals.empty() && index.normal_index >= 0)
		{
			const float *np = &attrib.normals[3 * index.normal_index];
			vertex.normal   = {*(np + 0), *(np + 1), *(np + 2)};
		}

		if (!attrib.texcoords.empty() && index.texcoord_index >= 0)
		{
			const float *tp = &attrib.texcoords[2 * index.texcoord_index + 0];
			vertex.uv       = {*tp, 1.0f - *(tp + 1)};
		}

		m_vertices.push_back(vertex);
		m_indices.push_back(static_cast<int>(m_indices.size()));
	}

	// Compute normal when no normal were provided.
	// if (attrib.normals.empty())
	//{
	//	for (size_t i = 0; i < m_indices.size(); i += 3)
	//	{
	//		Vertex& v0 = m_vertices[m_indices[i + 0]];
	//		Vertex& v1 = m_vertices[m_indices[i + 1]];
	//		Vertex& v2 = m_vertices[m_indices[i + 2]];

	//		nvmath::vec3f n = nvmath::normalize(nvmath::cross(
	//			(v1.position - v0.position),
	//			(v2.position - v0.position)));
	//		v0.normal = n;
	//		v1.normal = n;
	//		v2.normal = n;
	//	}
	//}
}

void MeshAlloc::init(ContextAware *pContext, Mesh *pMesh)
{
	auto             &m_alloc              = pContext->m_alloc;
	auto              m_device             = pContext->getDevice();
	auto              m_graphicsQueueIndex = pContext->getQueueFamily();
	nvvk::CommandPool cmdGen(m_device, m_graphicsQueueIndex);
	auto              cmdBuf = cmdGen.createCommandBuffer();

	init(pContext, pMesh, cmdBuf);

	cmdGen.submitAndWait(cmdBuf);
	m_alloc.finalizeAndReleaseStaging();
}

void MeshAlloc::init(ContextAware *pContext, Mesh *pMesh, const VkCommandBuffer &cmdBuf)
{
	auto &m_alloc = pContext->m_alloc;

	m_nIndices  = static_cast<uint32_t>(pMesh->m_indices.size());
	m_nVertices = static_cast<uint32_t>(pMesh->m_vertices.size());

	VkBufferUsageFlags flag            = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
	VkBufferUsageFlags rayTracingFlags =        // used also for building acceleration structures
	    flag | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	m_bVertices = m_alloc.createBuffer(cmdBuf, pMesh->m_vertices, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | rayTracingFlags);
	m_bIndices  = m_alloc.createBuffer(cmdBuf, pMesh->m_indices, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | rayTracingFlags);
}

void MeshAlloc::deinit(ContextAware *pContext)
{
	auto &m_alloc = pContext->m_alloc;

	m_alloc.destroy(m_bVertices);
	m_alloc.destroy(m_bIndices);
}

void SceneDescAlloc::init(ContextAware *pContext, const std::map<uint32_t, MeshAlloc *> &meshAllocLUT, const VkCommandBuffer &cmdBuf)
{
	auto m_device = pContext->getDevice();

	for (auto &record : meshAllocLUT)
	{
		MeshAlloc *pMeshAlloc = record.second;
		MeshDesc   desc;
		desc.vertexAddress = nvvk::getBufferDeviceAddress(m_device, pMeshAlloc->m_bVertices.buffer);
		desc.indexAddress  = nvvk::getBufferDeviceAddress(m_device, pMeshAlloc->m_bIndices.buffer);
		m_sceneDesc.emplace_back(desc);
	}
	m_bSceneDesc = pContext->m_alloc.createBuffer(cmdBuf, m_sceneDesc, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
}

void SceneDescAlloc::deinit(ContextAware *pContext)
{
	pContext->m_alloc.destroy(m_bSceneDesc);
}
