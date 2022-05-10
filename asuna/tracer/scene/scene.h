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

class Scene
{
  public:
    void init(ContextAware *pContext);
    void create(std::string sceneFilePath);
    void deinit();

  private:
    ContextAware *m_pContext = nullptr;
    std::string   m_sceneFileDir;

    // Integrator
  private:
    Integrator *m_pIntegrator = nullptr;
    void        addIntegrator(const nlohmann::json &integratorJson);

  public:
    VkExtent2D getSize();
    uint32_t   getSpp();
    uint32_t   getMaxRecurDepth();

    // Camera
  private:
    CameraInterface *m_pCamera = nullptr;
    void             addCamera(const nlohmann::json &cameraJson);
    void             addCameraPerspective(const nlohmann::json &cameraJson);
    void             addCameraPinhole(const nlohmann::json &cameraJson);

  public:
    CameraInterface *getCamera();
    CameraType       getCameraType();

    // Emitter
  private:
    std::vector<GPUEmitter> m_emitters      = {};
    EmitterAlloc        *m_pEmitterAlloc = nullptr;
    void                 addEmitter(const nlohmann::json &emitterJson);
    void                 addEmitterDistant(const nlohmann::json &emitterJson);
    void                 allocEmitters(ContextAware *pContext, const VkCommandBuffer &cmdBuf);

  public:
    EmitterAlloc *getEmitterAlloc();
    int           getEmittersNum();

    // Texture
  private:
    std::map<std::string, std::pair<Texture *, uint32_t>> m_textureLUT{};
    std::map<uint32_t, TextureAlloc *>                    m_textureAllocLUT{};
    void addTexture(const nlohmann::json &textureJson);
    void allocTexture(ContextAware *pContext, uint32_t textureId, const std::string &textureName,
                      Texture *pTexture, const VkCommandBuffer &cmdBuf);

  public:
    uint32_t      getTexturesNum();
    uint32_t      getTextureId(const std::string &textureName);
    TextureAlloc *getTextureAlloc(uint32_t textureId);

    // Material
  private:
    std::map<std::string, std::pair<MaterialInterface *, uint32_t>> m_materialLUT{};
    std::map<uint32_t, MaterialAlloc *>                             m_materialAllocLUT{};
    void addMaterial(const nlohmann::json &materialJson);
    void addMaterialBrdfHongzhi(const nlohmann::json &materialJson);
    void addMaterialBrdfLambertian(const nlohmann::json &materialJson);
    void allocMaterial(ContextAware *pContext, uint32_t materialId,
                       const std::string &materialName, MaterialInterface *pMaterial,
                       const VkCommandBuffer &cmdBuf);

  public:
    uint32_t       getMaterialsNum();
    uint32_t       getMaterialId(const std::string &materialName);
    MaterialAlloc *getMaterialAlloc(uint32_t materialId);

    // Mesh
  private:
    // meshName -> { pMesh, meshId }
    std::map<std::string, std::pair<Mesh *, uint32_t>> m_meshLUT{};
    std::map<uint32_t, MeshAlloc *>                    m_meshAllocLUT{};
    void                                               addMesh(const nlohmann::json &meshJson);
    void allocMesh(ContextAware *pContext, uint32_t meshId, const std::string &meshName,
                   Mesh *pMesh, const VkCommandBuffer &cmdBuf);

  public:
    uint32_t   getMeshesNum();
    uint32_t   getMeshId(const std::string &meshName);
    MeshAlloc *getMeshAlloc(uint32_t meshId);

    // Instance
  private:
    std::vector<Instance *> m_instances{};
    void                    addInstance(const nlohmann::json &instanceJson);

  public:
    uint32_t                       getInstancesNum();
    const std::vector<Instance *> &getInstances();

    // SceneDesc
  private:
    SceneDescAlloc *m_pSceneDescAlloc = nullptr;
    void            allocSceneDesc(ContextAware *pContext, const VkCommandBuffer &cmdBuf);

  public:
    VkBuffer getSceneDescBuffer();

    // Shot
  private:
    std::vector<CameraShot> m_shots{};
    void                    addShot(const nlohmann::json &shotJson);

  private:
    void parseSceneFile(std::string sceneFilePath);
    void computeSceneDimensions();
    void fitCamera();
    void addDummyTexture();
    void addDummyMaterial();
    void allocScene();
    void freeRawData();
    void freeAllocData();

  private:
    struct Dimensions
    {
        nvmath::vec3f min = nvmath::vec3f(std::numeric_limits<float>::max());
        nvmath::vec3f max = nvmath::vec3f(std::numeric_limits<float>::min());
        nvmath::vec3f size{0.f};
        nvmath::vec3f center{0.f};
        float         radius{0};
    } m_dimensions;
};