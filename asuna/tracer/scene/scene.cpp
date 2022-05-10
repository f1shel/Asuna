#include "scene.h"

#include <nvmath/nvmath.h>
#include <nvh/fileoperations.hpp>
#include <nvh/timesampler.hpp>
#include <nvvk/buffers_vk.hpp>
#include <nvvk/commands_vk.hpp>

#include <filesystem>
#include <fstream>

void Scene::init(ContextAware *pContext)
{
    m_pContext = pContext;
    reset();
}

void Scene::deinit()
{
    if (m_pContext == nullptr)
    {
        LOGE("[x] %-20s: failed to find belonging context when deinit.", "Scene Error");
        exit(1);
    }
    freeRawData();
    freeAllocData();
}

void Scene::submit()
{
    nvvk::CommandPool cmdBufGet(m_pContext->getDevice(), m_pContext->getQueueFamily());
    VkCommandBuffer   cmdBuf = cmdBufGet.createCommandBuffer();

    allocEmitters(m_pContext, cmdBuf);

    for (auto &record : m_pTextures)
    {
        const auto &textureName = record.first;
        const auto &valuePair   = record.second;
        auto        pTexture    = valuePair.first;
        auto        textureId   = valuePair.second;
        allocTexture(m_pContext, textureId, textureName, pTexture, cmdBuf);
    }

    for (auto &record : m_pMaterials)
    {
        const auto &materialName = record.first;
        const auto &valuePair    = record.second;
        auto        pMaterial    = valuePair.first;
        auto        materialId   = valuePair.second;
        allocMaterial(m_pContext, materialId, materialName, pMaterial, cmdBuf);
    }

    for (auto &record : m_pMeshes)
    {
        const auto &meshName  = record.first;
        const auto &valuePair = record.second;
        auto        pMesh     = valuePair.first;
        auto        meshId    = valuePair.second;
        allocMesh(m_pContext, meshId, meshName, pMesh, cmdBuf);
    }

    // Keeping the mesh description at host and device
    allocSceneDesc(m_pContext, cmdBuf);

    allocSunAndSky(m_pContext, cmdBuf);

    cmdBufGet.submitAndWait(cmdBuf);
    m_pContext->m_alloc.finalizeAndReleaseStaging();

    // autofit
    if (m_shots.empty())
    {
        computeSceneDimensions();
        fitCamera();
        auto cam = CameraManip.getCamera();
        m_shots.emplace_back(CameraShot{cam.ctr, cam.eye, cam.up});
    }
    m_pCamera->setToWorld(m_shots[0]);

    m_hasScene = true;
}

void Scene::reset()
{
    if (m_hasScene)
        freeAllocData();
    freeRawData();
    m_hasScene = false;
    // Add dummy texture and material so that pipeline
    // compilation will not complain
    const std::string &tn        = "add_by_default_dummy_texture";
    const std::string &mn        = "add_by_default_dummy_material";
    Texture           *pTexture  = new Texture;
    Material          *pMaterial = new Material;
    m_pTextures[tn]              = std::make_pair(pTexture, m_pTextures.size());
    m_pMaterials[mn]             = std::make_pair(pMaterial, m_pMaterials.size());
    m_sunAndSky                  = {
        {1, 1, 1},                  // rgb_unit_conversion;
        0.0000101320f,              // multiplier;
        0.0f,                       // haze;
        0.0f,                       // redblueshift;
        1.0f,                       // saturation;
        0.0f,                       // horizon_height;
        {0.4f, 0.4f, 0.4f},         // ground_color;
        0.1f,                       // horizon_blur;
        {0.0, 0.0, 0.01f},          // night_color;
        0.8f,                       // sun_disk_intensity;
        {0.00, 0.78, 0.62f},        // sun_direction;
        5.0f,                       // sun_disk_scale;
        1.0f,                       // sun_glow_intensity;
        1,                          // y_is_up;
        1,                          // physically_scaled_sun;
        0,                          // in_use;
    };
}

void Scene::freeAllocData()
{
    m_pEmittersAlloc->deinit(m_pContext);
    delete m_pEmittersAlloc;
    m_pEmittersAlloc = nullptr;

    // free textures alloc data
    for (auto &record : m_pTexturesAlloc)
    {
        auto pTextureAlloc = record.second;
        pTextureAlloc->deinit(m_pContext);
    }

    // free meshes alloc data
    for (auto &record : m_pMeshesAlloc)
    {
        auto pMeshAlloc = record.second;
        pMeshAlloc->deinit(m_pContext);
    }

    // free meshes alloc data
    for (auto &record : m_pMaterialsAlloc)
    {
        auto pMaterialAlloc = record.second;
        pMaterialAlloc->deinit(m_pContext);
    }

    // free scene desc alloc data
    m_pSceneDescAlloc->deinit(m_pContext);
    delete m_pSceneDescAlloc;
    m_pSceneDescAlloc = nullptr;

    // free sun and sky
    m_pContext->m_alloc.destroy(m_bSunAndSky);
}

void Scene::freeRawData()
{
    delete m_pIntegrator;
    m_pIntegrator = nullptr;

    delete m_pCamera;
    m_pCamera = nullptr;

    m_emitters.clear();

    for (auto &record : m_pTextures)
    {
        const auto &valuePair = record.second;
        auto        pTexture  = valuePair.first;
        delete pTexture;
    }
    m_pTextures.clear();

    for (auto &record : m_pMeshes)
    {
        const auto &valuePair = record.second;
        auto        pMesh     = valuePair.first;
        delete pMesh;
    }
    m_pMeshes.clear();

    for (auto &record : m_pMaterials)
    {
        const auto &valuePair = record.second;
        auto        pMaterial = valuePair.first;
        delete pMaterial;
    }
    m_pMaterials.clear();

    // free instance raw data
    for (auto &pInst : m_pInstances)
    {
        delete pInst;
        pInst = nullptr;
    }
    m_pInstances.clear();

    m_shots.clear();
}

void Scene::addIntegrator(VkExtent2D size, int spp, int maxRecur)
{
    m_pIntegrator = new Integrator(size, spp, maxRecur);
}

void Scene::addCamera(CameraType camType, nvmath::vec4f fxfycxcy, float fov)
{
    assert(m_pCamera == nullptr && "already have a camera!");
    m_pCamera = new Camera(camType, fxfycxcy, fov);
}

void Scene::addEmitter(const GPUEmitter &emitter)
{
    m_emitters.emplace_back(emitter);
}

void Scene::addTexture(const std::string &textureName, const std::string &texturePath,
                       float gamma)
{
    Texture *pTexture        = new Texture(texturePath, gamma);
    m_pTextures[textureName] = std::make_pair(pTexture, m_pTextures.size());
}

void Scene::addMaterial(const std::string &materialName, const GPUMaterial &material)
{
    Material *pMaterial        = new Material;
    pMaterial->m_material      = material;
    m_pMaterials[materialName] = std::make_pair(pMaterial, m_pMaterials.size());
}

void Scene::addMesh(const std::string &meshName, const std::string &meshPath,
                    bool recomputeNormal)
{
    Mesh *pMesh         = new Mesh(meshPath, recomputeNormal);
    m_pMeshes[meshName] = std::make_pair(pMesh, m_pMeshes.size());
}

void Scene::addInstance(const nvmath::mat4f &transform, const std::string &meshName,
                        const std::string &materialName)
{
    Instance *pInst = new Instance(transform, getMeshId(meshName), getMaterialId(materialName));
    m_pInstances.emplace_back(pInst);
}

void Scene::addShot(const nvmath::vec3f &eye, const nvmath::vec3f &lookat,
                    const nvmath::vec3f &up)
{
    m_shots.emplace_back(CameraShot{lookat, eye, up});
}

int Scene::getMeshId(const std::string &meshName)
{
    if (m_pMeshes.count(meshName))
        return m_pMeshes[meshName].second;
    else
    {
        LOGE("[x] %-20s: mesh %s does not exist\n", "Scene Error", meshName.c_str());
        exit(1);
    }
    return 0;
}

int Scene::getTextureId(const std::string &textureName)
{
    if (m_pTextures.count(textureName))
        return m_pTextures[textureName].second;
    else
    {
        LOGE("[x] %-20s: texture %s does not exist\n", "Scene Error", textureName.c_str());
        exit(1);
    }
    return 0;
}

int Scene::getMaterialId(const std::string &materialName)
{
    if (m_pMaterials.count(materialName))
        return m_pMaterials[materialName].second;
    else
    {
        LOGE("[x] %-20s: material %s does not exist\n", "Scene Error", materialName.c_str());
        exit(1);
    }
    return 0;
}

int Scene::getMeshesNum()
{
    return m_pMeshes.size();
}

int Scene::getInstancesNum()
{
    return m_pInstances.size();
}

int Scene::getTexturesNum()
{
    return m_pTextures.size();
}

int Scene::getMaterialsNum()
{
    return m_pMaterials.size();
}

int Scene::getEmittersNum()
{
    return m_emitters.size();
}

int Scene::getSpp()
{
    return m_pIntegrator->getSpp();
}

int Scene::getMaxPathDepth()
{
    return m_pIntegrator->getMaxPathDepth();
}

Camera *Scene::getCameraPtr()
{
    return m_pCamera;
}

CameraType Scene::getCameraType()
{
    return m_pCamera->getType();
}

nvvk::RaytracingBuilderKHR::BlasInput Scene::getBlas(VkDevice device, int meshId)
{
    return MeshBufferToBlas(device, *m_pMeshesAlloc[meshId]);
}

const InstancePtrs &Scene::getInstancePtrs()
{
    return m_pInstances;
}

VkExtent2D Scene::getSize()
{
    return m_pIntegrator->getSize();
}

VkBuffer Scene::getSceneDescDescriptor()
{
    return m_pSceneDescAlloc->getBuffer();
}

VkDescriptorImageInfo Scene::getTextureDescriptor(int textureId)
{
    return m_pTexturesAlloc[textureId]->getTexture().descriptor;
}

VkBuffer Scene::getEmittersDescriptor()
{
    return m_pEmittersAlloc->getBuffer();
}

VkBuffer Scene::getSunAndSkyDescriptor()
{
    return m_bSunAndSky.buffer;
}

SunAndSky &Scene::getSunAndSky()
{
    return m_sunAndSky;
}

void Scene::allocEmitters(ContextAware *pContext, const VkCommandBuffer &cmdBuf)
{
    m_pEmittersAlloc = new EmitterAlloc(pContext, m_emitters, cmdBuf);
}

void Scene::allocTexture(ContextAware *pContext, uint32_t textureId,
                         const std::string &textureName, Texture *pTexture,
                         const VkCommandBuffer &cmdBuf)
{
    auto &m_debug  = pContext->m_debug;
    auto  m_device = pContext->getDevice();

    TextureAlloc *pTextureAlloc = new TextureAlloc(pContext, pTexture, cmdBuf);
    m_pTexturesAlloc[textureId] = pTextureAlloc;
}

void Scene::allocMaterial(ContextAware *pContext, uint32_t materialId,
                          const std::string &materialName, Material *pMaterial,
                          const VkCommandBuffer &cmdBuf)
{
    auto &m_debug  = pContext->m_debug;
    auto  m_device = pContext->getDevice();

    MaterialAlloc *pMaterialAlloc = new MaterialAlloc(pContext, pMaterial, cmdBuf);
    m_pMaterialsAlloc[materialId] = pMaterialAlloc;

    m_debug.setObjectName(pMaterialAlloc->getBuffer(),
                          std::string(materialName + "_materialBuffer"));
}

void Scene::allocMesh(ContextAware *pContext, uint32_t meshId, const std::string &meshName,
                      Mesh *pMesh, const VkCommandBuffer &cmdBuf)
{
    auto &m_debug  = pContext->m_debug;
    auto  m_device = pContext->getDevice();

    MeshAlloc *pMeshAlloc  = new MeshAlloc(pContext, pMesh, cmdBuf);
    m_pMeshesAlloc[meshId] = pMeshAlloc;

    m_debug.setObjectName(pMeshAlloc->getVerticesBuffer(),
                          std::string(meshName + "_vertexBuffer"));
    m_debug.setObjectName(pMeshAlloc->getIndicesBuffer(),
                          std::string(meshName + "_indexBuffer"));
}

void Scene::allocSceneDesc(ContextAware *pContext, const VkCommandBuffer &cmdBuf)
{
    // Keeping the obj host model and device description
    m_pSceneDescAlloc =
        new SceneDescAlloc(pContext, m_pInstances, m_pMeshesAlloc, m_pMaterialsAlloc, cmdBuf);
}

void Scene::allocSunAndSky(ContextAware *pContext, const VkCommandBuffer &cmdBuf)
{
    auto &m_alloc = pContext->m_alloc;
    auto &m_debug = pContext->m_debug;
    m_bSunAndSky  = m_alloc.createBuffer(
         sizeof(SunAndSky), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    NAME_VK(m_bSunAndSky.buffer);
}

void Scene::computeSceneDimensions()
{
    Bbox scnBbox;

    for (const auto pInstance : m_pInstances)
    {
        auto pMeshAlloc = m_pMeshesAlloc[pInstance->m_meshIndex];
        Bbox bbox(pMeshAlloc->m_posMin, pMeshAlloc->m_posMax);
        bbox.transform(pInstance->m_transform);
        scnBbox.insert(bbox);
    }

    if (scnBbox.isEmpty() || !scnBbox.isVolume())
    {
        LOGE("[!] %-20s: Scene bounding box invalid, Setting to: [-1,-1,-1], [1,1,1]",
             "Scene Warning");
        scnBbox.insert({-1.0f, -1.0f, -1.0f});
        scnBbox.insert({1.0f, 1.0f, 1.0f});
    }

    m_dimensions.min    = scnBbox.min();
    m_dimensions.max    = scnBbox.max();
    m_dimensions.size   = scnBbox.extents();
    m_dimensions.center = scnBbox.center();
    m_dimensions.radius = scnBbox.radius();
}

void Scene::fitCamera()
{
    auto m_size = m_pIntegrator->getSize();
    CameraManip.fit(m_dimensions.min, m_dimensions.max, true, false,
                    m_size.width / static_cast<float>(m_size.height));
}
