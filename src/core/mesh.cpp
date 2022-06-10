#include "mesh.h"

#include <nvh/nvprint.hpp>
#include <nvvk/buffers_vk.hpp>
#include <nvvk/commands_vk.hpp>
#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

static void updateAabb(const GpuVertex& v, vec3& posMin, vec3& posMax) {
  posMin.x = std::min(posMin.x, v.pos.x);
  posMin.y = std::min(posMin.y, v.pos.y);
  posMin.z = std::min(posMin.z, v.pos.z);
  posMax.x = std::max(posMax.x, v.pos.x);
  posMax.y = std::max(posMax.y, v.pos.y);
  posMax.z = std::max(posMax.z, v.pos.z);
}

Mesh::Mesh(Primitive& prim) {
  if (prim.type == PrimitiveTypeRect) {
    m_vertices.resize(4);
    m_indices.resize(6);
    /*
     * 0________1
     * |        |
     * |________|
     * 3        2
     */
    m_vertices[0].pos = prim.position;
    m_vertices[1].pos = prim.position + prim.u;
    m_vertices[2].pos = prim.position + prim.u + prim.v;
    m_vertices[3].pos = prim.position + prim.v;
    m_indices = {0, 1, 2, 0, 2, 3};

  } else if (prim.type == PrimitiveTypeTriangle) {
    m_vertices.resize(3);
    m_indices.resize(3);
    /*
     * 0__1
     * | /
     * |/
     * 2
     */
    m_vertices[0].pos = prim.position;
    m_vertices[1].pos = prim.position + prim.u;
    m_vertices[2].pos = prim.position + prim.v;
    m_indices = {0, 1, 2};
  }
  for (auto& i : m_vertices) updateAabb(i, m_posMin, m_posMax);
}

Mesh::Mesh(const std::string& meshPath, bool recomputeNormal, vec2 uvScale) {
  m_vertices.clear();
  m_indices.clear();
  loadMesh(meshPath, m_vertices, m_indices);

  for (size_t i = 0; i < m_indices.size(); i += 3) {
    GpuVertex& v0 = m_vertices[m_indices[i + 0]];
    GpuVertex& v1 = m_vertices[m_indices[i + 1]];
    GpuVertex& v2 = m_vertices[m_indices[i + 2]];

    // if (recomputeNormal) {
    nvmath::vec3f n =
        nvmath::normalize(nvmath::cross((v1.pos - v0.pos), (v2.pos - v0.pos)));
    v0.normal = n;
    v1.normal = n;
    v2.normal = n;
    //}

    v0.uv = uvScale * v0.uv;
    v1.uv = uvScale * v1.uv;
    v2.uv = uvScale * v2.uv;

    updateAabb(v0, m_posMin, m_posMax);
    updateAabb(v1, m_posMin, m_posMax);
    updateAabb(v2, m_posMin, m_posMax);
  }
}

MeshAlloc::MeshAlloc(ContextAware* pContext, Mesh* pMesh,
                     const VkCommandBuffer& cmdBuf) {
  auto& m_alloc = pContext->getAlloc();

  m_posMin = pMesh->getPosMin();
  m_posMax = pMesh->getPosMax();

  m_numIndices = static_cast<uint32_t>(pMesh->getIndicesNum());
  m_numVertices = static_cast<uint32_t>(pMesh->getVerticesNum());

  VkBufferUsageFlags flag = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
  VkBufferUsageFlags
      rayTracingFlags =  // used also for building acceleration structures
      flag |
      VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
  m_bVertices =
      m_alloc.createBuffer(cmdBuf, pMesh->getVertices(),
                           VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | rayTracingFlags);
  m_bIndices =
      m_alloc.createBuffer(cmdBuf, pMesh->getIndices(),
                           VK_BUFFER_USAGE_INDEX_BUFFER_BIT | rayTracingFlags);
}

void MeshAlloc::deinit(ContextAware* pContext) {
  auto& m_alloc = pContext->getAlloc();

  m_alloc.destroy(m_bVertices);
  m_alloc.destroy(m_bIndices);

  intoReleased();
}

void loadMesh(const std::string& meshPath, vector<GpuVertex>& vertices,
              vector<uint>& indices) {
  tinyobj::ObjReader reader;
  reader.ParseFromFile(meshPath);
  if (!reader.Valid()) {
    LOG_ERROR("{}: load mesh from [{}] --- {}", "Scene", meshPath.c_str(),
              reader.Error().c_str());
    exit(1);
  }
  const auto& shapes = reader.GetShapes();
  const auto& attrib = reader.GetAttrib();
  for (const auto& shape : shapes) {
    vertices.reserve(vertices.size() + shape.mesh.indices.size());
    indices.reserve(indices.size() + shape.mesh.indices.size());
    for (const auto& index : shape.mesh.indices) {
      GpuVertex vertex = {};
      const float* vp = &attrib.vertices[3 * index.vertex_index];
      vertex.pos = {*(vp + 0), *(vp + 1), *(vp + 2)};

      if (!attrib.texcoords.empty() && index.texcoord_index >= 0) {
        const float* tp = &attrib.texcoords[2 * index.texcoord_index + 0];
        vertex.uv = {*tp, 1.0f - *(tp + 1)};
      }

      if (!attrib.normals.empty() && index.normal_index >= 0) {
        const float* np = &attrib.normals[3 * index.normal_index];
        vertex.normal = {*(np + 0), *(np + 1), *(np + 2)};
      }

      vertices.push_back(vertex);
      indices.push_back(static_cast<int>(indices.size()));
    }
  }
}

nvvk::RaytracingBuilderKHR::BlasInput MeshBufferToBlas(VkDevice device,
                                                       MeshAlloc& meshAlloc) {
  // BLAS builder requires raw device addresses.
  VkDeviceAddress vertexAddress =
      nvvk::getBufferDeviceAddress(device, meshAlloc.getVerticesBuffer());
  VkDeviceAddress indexAddress =
      nvvk::getBufferDeviceAddress(device, meshAlloc.getIndicesBuffer());

  uint maxPrimitiveCount = meshAlloc.getIndicesNum() / 3;

  // Describe buffer as array of Vertex.
  VkAccelerationStructureGeometryTrianglesDataKHR triangles{
      VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR};
  triangles.vertexFormat =
      VK_FORMAT_R32G32B32_SFLOAT;  // vec3 vertex position data.
  triangles.vertexData.deviceAddress = vertexAddress;
  triangles.vertexStride = sizeof(GpuVertex);
  // Describe index data (32-bit unsigned int)
  triangles.indexType = VK_INDEX_TYPE_UINT32;
  triangles.indexData.deviceAddress = indexAddress;
  // Indicate identity transform by setting transformData to null device
  // pointer. triangles.transformData = {};
  triangles.maxVertex = meshAlloc.getVerticesNum();

  // Identify the above data as containing opaque triangles.
  VkAccelerationStructureGeometryKHR asGeom{
      VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
  asGeom.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
  asGeom.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
  asGeom.geometry.triangles = triangles;

  // The entire array will be used to build the BLAS.
  VkAccelerationStructureBuildRangeInfoKHR offset;
  offset.firstVertex = 0;
  offset.primitiveCount = maxPrimitiveCount;
  offset.primitiveOffset = 0;
  offset.transformOffset = 0;

  // Our blas is made from only one geometry, but could be made of many
  // geometries
  nvvk::RaytracingBuilderKHR::BlasInput input;
  input.asGeometry.emplace_back(asGeom);
  input.asBuildOffsetInfo.emplace_back(offset);

  return input;
}