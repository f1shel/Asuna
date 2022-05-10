#pragma once

#include <nvvk/buffers_vk.hpp>
#include "../../hostdevice/emitter.h"
#include "../../hostdevice/scene.h"
#include "../context/context.h"
#include "emitter.h"
#include "instance.h"
#include "integrator.h"
#include "json/json.hpp"
#include "material.h"
#include "mesh.h"
#include "texture.h"

#include <map>
#include <string>
#include <vector>

class SceneDescAlloc : public GPUAlloc
{
  public:
    SceneDescAlloc(ContextAware *pContext, const std::vector<Instance *> &instances,
                   std::map<uint32_t, MeshAlloc *>     &meshAllocLUT,
                   std::map<uint32_t, MaterialAlloc *> &materialAllocLUT,
                   const VkCommandBuffer               &cmdBuf)
    {
        auto &m_alloc  = pContext->m_alloc;
        auto  m_device = pContext->getDevice();
        for (auto pInstance : instances)
        {
            auto            pMeshAlloc     = meshAllocLUT[pInstance->m_meshIndex];
            auto            pMaterialAlloc = materialAllocLUT[pInstance->m_materialIndex];
            GPUInstanceDesc desc;
            desc.vertexAddress =
                nvvk::getBufferDeviceAddress(m_device, pMeshAlloc->getVerticesBuffer());
            desc.indexAddress =
                nvvk::getBufferDeviceAddress(m_device, pMeshAlloc->getIndicesBuffer());
            desc.materialAddress =
                nvvk::getBufferDeviceAddress(m_device, pMaterialAlloc->getBuffer());
            m_sceneDesc.emplace_back(desc);
        }
        m_bSceneDesc =
            m_alloc.createBuffer(cmdBuf, m_sceneDesc, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    }
    void deinit(ContextAware *pContext)
    {
        pContext->m_alloc.destroy(m_bSceneDesc);
        intoReleased();
    }
    VkBuffer getBuffer()
    {
        return m_bSceneDesc.buffer;
    }

  private:
    std::vector<GPUInstanceDesc> m_sceneDesc{};
    nvvk::Buffer                 m_bSceneDesc;
};

using Emitters          = std::vector<GPUEmitter>;
using TexturePtrs       = std::map<std::string, std::pair<Texture *, uint32_t>>;
using MeshPtrs          = std::map<std::string, std::pair<Mesh *, uint32_t>>;
using MaterialPtrs      = std::map<std::string, std::pair<Material *, uint32_t>>;
using InstancePtrs      = std::vector<Instance *>;
using Shots             = std::vector<CameraShot>;
using MeshAllocPtrs     = std::map<uint32_t, MeshAlloc *>;
using MaterialAllocPtrs = std::map<uint32_t, MaterialAlloc *>;
using TextureAllocPtrs  = std::map<uint32_t, TextureAlloc *>;

class Scene
{
  public:
    void init(ContextAware *pContext);
    void deinit();
    void submit();
    void reset();
    void freeAllocData();
    void freeRawData();

  public:
    void addIntegrator(VkExtent2D size, int spp, int maxRecur);
    void addCamera(CameraType camType, nvmath::vec4f fxfycxcy, float fov);
    void addEmitter(const GPUEmitter &emitter);
    void addTexture(const std::string &textureName, const std::string &texturePath, float gamma);
    void addMaterial(const std::string &materialName, const GPUMaterial &material);
    void addMesh(const std::string &meshName, const std::string &meshPath, bool recomputeNormal);
    void addInstance(const nvmath::mat4f &transform, const std::string &meshName,
                     const std::string &materialName);
    void addShot(const nvmath::vec3f &eye, const nvmath::vec3f &lookat, const nvmath::vec3f &up);

  public:
    int getMeshId(const std::string &meshName);
    int getTextureId(const std::string &textureName);
    int getMaterialId(const std::string &materialName);
    int getMeshesNum();
    int getInstancesNum();
    int getTexturesNum();
    int getMaterialsNum();
    int getEmittersNum();
    int getSpp();
    int getMaxPathDepth();

  public:
    Camera                               *getCameraPtr();
    CameraType                            getCameraType();
    nvvk::RaytracingBuilderKHR::BlasInput getBlas(VkDevice device, int meshId);
    const InstancePtrs                   &getInstancePtrs();
    VkExtent2D                            getSize();
    VkBuffer                              getSceneDescDescriptor();
    VkDescriptorImageInfo                 getTextureDescriptor(int textureId);
    VkBuffer                              getEmittersDescriptor();

  private:
    std::string   m_sceneFileDir = "";
    ContextAware *m_pContext     = nullptr;
    bool          m_hasScene     = false;
    // ---------------- CPU resources ----------------
    Integrator  *m_pIntegrator = nullptr;
    Camera      *m_pCamera     = nullptr;
    Emitters     m_emitters    = {};
    TexturePtrs  m_pTextures   = {};
    MeshPtrs     m_pMeshes     = {};
    MaterialPtrs m_pMaterials  = {};
    InstancePtrs m_pInstances  = {};
    Shots        m_shots       = {};
    // ---------------- GPU resources ----------------
    EmitterAlloc     *m_pEmittersAlloc  = nullptr;
    TextureAllocPtrs  m_pTexturesAlloc  = {};
    MaterialAllocPtrs m_pMaterialsAlloc = {};
    MeshAllocPtrs     m_pMeshesAlloc    = {};
    SceneDescAlloc   *m_pSceneDescAlloc = nullptr;
    // ---------------- ------------- ----------------
    struct Dimensions
    {
        nvmath::vec3f min = nvmath::vec3f(std::numeric_limits<float>::max());
        nvmath::vec3f max = nvmath::vec3f(std::numeric_limits<float>::min());
        nvmath::vec3f size{0.f};
        nvmath::vec3f center{0.f};
        float         radius{0};
    } m_dimensions;

  private:
    void allocEmitters(ContextAware *pContext, const VkCommandBuffer &cmdBuf);
    void allocTexture(ContextAware *pContext, uint32_t textureId, const std::string &textureName,
                      Texture *pTexture, const VkCommandBuffer &cmdBuf);
    void allocMaterial(ContextAware *pContext, uint32_t materialId,
                       const std::string &materialName, Material *pMaterial,
                       const VkCommandBuffer &cmdBuf);
    void allocMesh(ContextAware *pContext, uint32_t meshId, const std::string &meshName,
                   Mesh *pMesh, const VkCommandBuffer &cmdBuf);
    void allocSceneDesc(ContextAware *pContext, const VkCommandBuffer &cmdBuf);
    void computeSceneDimensions();
    void fitCamera();
};