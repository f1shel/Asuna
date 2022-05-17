#include "mesh.h"

#include <nvh/nvprint.hpp>
#include <nvvk/buffers_vk.hpp>
#include <nvvk/commands_vk.hpp>
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

Mesh::Mesh(Primitive& prim)
{
  if(prim.type == PrimitiveTypeRect)
  {
    m_vertices.reserve(4);
    m_indices.reserve(6);
    GpuVertex v[4];
    /*
         * 0________1
         * |        |
         * |________|
         * 3        2
         */
    v[0].pos  = prim.position;
    v[1].pos  = prim.position + prim.u;
    v[2].pos  = prim.position + prim.u + prim.v;
    v[3].pos  = prim.position + prim.v;
    m_indices = {0, 1, 2, 0, 2, 3};
    for(auto& i : v)
    {
      m_posMin.x = std::min(m_posMin.x, i.pos.x);
      m_posMin.y = std::min(m_posMin.y, i.pos.y);
      m_posMin.z = std::min(m_posMin.z, i.pos.z);
      m_posMax.x = std::max(m_posMax.x, i.pos.x);
      m_posMax.y = std::max(m_posMax.y, i.pos.y);
      m_posMax.z = std::max(m_posMax.z, i.pos.z);
      m_vertices.emplace_back(i);
    }
  }
}

Mesh::Mesh(const std::string& meshPath, bool recomputeNormal)
{
  tinyobj::ObjReader reader;
  reader.ParseFromFile(meshPath);
  if(!reader.Valid())
  {
    LOGE("[x] %-20s: load mesh from %s ----> %s\n", "Scene Error", meshPath.c_str(), reader.Error().c_str());
    exit(1);
  }
  //if(reader.GetShapes().size() != 1)
  //{
  //  LOGE(
  //      "[x] %-20s: load mesh from %s ----> asuna tracer supports only one shape "
  //      "per mesh\n",
  //      "Scene Error", meshPath.c_str());
  //  exit(1);
  //}
  m_vertices.clear();
  m_indices.clear();
  const auto& shapes = reader.GetShapes();
  const auto& attrib = reader.GetAttrib();
  for(const auto& shape : shapes)
  {
    m_vertices.reserve(m_vertices.size() + shape.mesh.indices.size());
    m_indices.reserve(m_indices.size() + shape.mesh.indices.size());
    for(const auto& index : shape.mesh.indices)
    {
      GpuVertex    vertex = {};
      const float* vp     = &attrib.vertices[3 * index.vertex_index];
      vertex.pos          = {*(vp + 0), *(vp + 1), *(vp + 2)};

      if(!attrib.normals.empty() && index.normal_index >= 0)
      {
        const float* np = &attrib.normals[3 * index.normal_index];
        vertex.normal   = {*(np + 0), *(np + 1), *(np + 2)};
      }

      if(!attrib.texcoords.empty() && index.texcoord_index >= 0)
      {
        const float* tp = &attrib.texcoords[2 * index.texcoord_index + 0];
        vertex.uv       = {*tp, 1.0f - *(tp + 1)};
      }

      m_vertices.push_back(vertex);
      m_indices.push_back(static_cast<int>(m_indices.size()));

      m_posMin.x = std::min(m_posMin.x, vertex.pos.x);
      m_posMin.y = std::min(m_posMin.y, vertex.pos.y);
      m_posMin.z = std::min(m_posMin.z, vertex.pos.z);
      m_posMax.x = std::max(m_posMax.x, vertex.pos.x);
      m_posMax.y = std::max(m_posMax.y, vertex.pos.y);
      m_posMax.z = std::max(m_posMax.z, vertex.pos.z);
    }

    // Compute normal when no normal were provided or recomputing is required.
    if(attrib.normals.empty() || recomputeNormal)
    {
      for(size_t i = 0; i < m_indices.size(); i += 3)
      {
        GpuVertex& v0 = m_vertices[m_indices[i + 0]];
        GpuVertex& v1 = m_vertices[m_indices[i + 1]];
        GpuVertex& v2 = m_vertices[m_indices[i + 2]];

        nvmath::vec3f n = nvmath::normalize(nvmath::cross((v1.pos - v0.pos), (v2.pos - v0.pos)));
        v0.normal       = n;
        v1.normal       = n;
        v2.normal       = n;
      }
    }
  }
}

MeshAlloc::MeshAlloc(ContextAware* pContext, Mesh* pMesh, const VkCommandBuffer& cmdBuf)
{
  auto& m_alloc = pContext->getAlloc();

  m_posMin = pMesh->getPosMin();
  m_posMax = pMesh->getPosMax();

  m_numIndices  = static_cast<uint32_t>(pMesh->getIndicesNum());
  m_numVertices = static_cast<uint32_t>(pMesh->getVerticesNum());

  VkBufferUsageFlags flag            = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
  VkBufferUsageFlags rayTracingFlags =  // used also for building acceleration structures
      flag | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
  m_bVertices = m_alloc.createBuffer(cmdBuf, pMesh->getVertices(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | rayTracingFlags);
  m_bIndices  = m_alloc.createBuffer(cmdBuf, pMesh->getIndices(), VK_BUFFER_USAGE_INDEX_BUFFER_BIT | rayTracingFlags);
}

void MeshAlloc::deinit(ContextAware* pContext)
{
  auto& m_alloc = pContext->getAlloc();

  m_alloc.destroy(m_bVertices);
  m_alloc.destroy(m_bIndices);

  intoReleased();
}

nvvk::RaytracingBuilderKHR::BlasInput MeshBufferToBlas(VkDevice device, MeshAlloc& meshAlloc)
{
  // BLAS builder requires raw device addresses.
  VkDeviceAddress vertexAddress = nvvk::getBufferDeviceAddress(device, meshAlloc.getVerticesBuffer());
  VkDeviceAddress indexAddress  = nvvk::getBufferDeviceAddress(device, meshAlloc.getIndicesBuffer());

  uint maxPrimitiveCount = meshAlloc.getIndicesNum() / 3;

  // Describe buffer as array of Vertex.
  VkAccelerationStructureGeometryTrianglesDataKHR triangles{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR};
  triangles.vertexFormat             = VK_FORMAT_R32G32B32_SFLOAT;  // vec3 vertex position data.
  triangles.vertexData.deviceAddress = vertexAddress;
  triangles.vertexStride             = sizeof(GpuVertex);
  // Describe index data (32-bit unsigned int)
  triangles.indexType               = VK_INDEX_TYPE_UINT32;
  triangles.indexData.deviceAddress = indexAddress;
  // Indicate identity transform by setting transformData to null device pointer.
  // triangles.transformData = {};
  triangles.maxVertex = meshAlloc.getVerticesNum();

  // Identify the above data as containing opaque triangles.
  VkAccelerationStructureGeometryKHR asGeom{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
  asGeom.geometryType       = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
  asGeom.flags              = VK_GEOMETRY_OPAQUE_BIT_KHR;
  asGeom.geometry.triangles = triangles;

  // The entire array will be used to build the BLAS.
  VkAccelerationStructureBuildRangeInfoKHR offset;
  offset.firstVertex     = 0;
  offset.primitiveCount  = maxPrimitiveCount;
  offset.primitiveOffset = 0;
  offset.transformOffset = 0;

  // Our blas is made from only one geometry, but could be made of many geometries
  nvvk::RaytracingBuilderKHR::BlasInput input;
  input.asGeometry.emplace_back(asGeom);
  input.asBuildOffsetInfo.emplace_back(offset);

  return input;
}