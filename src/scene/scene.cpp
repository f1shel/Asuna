#include "scene.h"

#include <nvmath/nvmath.h>
#include <nvh/fileoperations.hpp>
#include <nvh/timesampler.hpp>
#include <nvvk/buffers_vk.hpp>
#include <nvvk/commands_vk.hpp>

#include <filesystem>
#include <fstream>

void Scene::init(ContextAware* pContext)
{
  m_pContext = pContext;
  reset();
}

void Scene::deinit()
{
  if(m_pContext == nullptr)
  {
    LOGE("[x] %-20s: failed to find belonging context when deinit.", "Scene Error");
    exit(1);
  }
  freeRawData();
  freeAllocData();
}

void Scene::submit()
{
  // configure pipeline state
  m_pipelineState.rtxState.numLights = getLightsNum() - 1;

  nvvk::CommandPool cmdBufGet(m_pContext->getDevice(), m_pContext->getQueueFamily());
  VkCommandBuffer   cmdBuf = cmdBufGet.createCommandBuffer();

  allocLights(m_pContext, cmdBuf);

  m_pTexturesAlloc.resize(getTexturesNum());
  for(auto& record : m_pTextures)
  {
    const auto& textureName = record.first;
    const auto& valuePair   = record.second;
    auto        pTexture    = valuePair.first;
    auto        textureId   = valuePair.second;
    allocTexture(m_pContext, textureId, textureName, pTexture, cmdBuf);
  }

  m_pMaterialsAlloc.resize(getMaterialsNum());
  for(auto& record : m_pMaterials)
  {
    const auto& materialName = record.first;
    const auto& valuePair    = record.second;
    auto        pMaterial    = valuePair.first;
    auto        materialId   = valuePair.second;
    allocMaterial(m_pContext, materialId, materialName, pMaterial, cmdBuf);
  }

  m_pMeshesAlloc.resize(getMeshesNum());
  for(auto& record : m_pMeshes)
  {
    const auto& meshName  = record.first;
    const auto& valuePair = record.second;
    auto        pMesh     = valuePair.first;
    auto        meshId    = valuePair.second;
    allocMesh(m_pContext, meshId, meshName, pMesh, cmdBuf);
  }

  allocEnvMap(m_pContext, cmdBuf);

  // Keeping the mesh description at host and device
  allocInstances(m_pContext, cmdBuf);

  allocSunAndSky(m_pContext, cmdBuf);

  cmdBufGet.submitAndWait(cmdBuf);
  m_pContext->getAlloc().finalizeAndReleaseStaging();

  // autofit
  if(m_shots.empty())
  {
    computeSceneDimensions();
    fitCamera();
  }
  setShot(0);

  m_hasScene = true;
}

void Scene::reset()
{
  if(m_hasScene)
    freeAllocData();
  freeRawData();
  m_hasScene = false;
  // Add dummy envmap, texture, material and light so that pipeline
  // compilation will not complain
  const std::string& tn        = "add_by_default_dummy_texture";
  const std::string& mn        = "add_by_default_dummy_material";
  Texture*           pTexture  = new Texture;
  Material*          pMaterial = new Material;
  m_pTextures[tn]              = std::make_pair(pTexture, m_pTextures.size());
  m_pMaterials[mn]             = std::make_pair(pMaterial, m_pMaterials.size());
  m_pEnvMap                    = new EnvMap;
  GpuLight defaultLight        = {
      LightTypeDirectional,  // type
      vec3(0.f),             // position
      vec3(0.f),             // direction
      vec3(0.f),             // emittance
      vec3(0.f),             // u
      vec3(0.f),             // v
      0.f,                   // radius
      0.f,                   // area
      1,                     // double side
  };
  addLight(defaultLight);
  m_sunAndSky = {
      {1, 1, 1},            // rgb_unit_conversion;
      0.0000101320f,        // multiplier;
      0.0f,                 // haze;
      0.0f,                 // redblueshift;
      1.0f,                 // saturation;
      0.0f,                 // horizon_height;
      {0.4f, 0.4f, 0.4f},   // ground_color;
      0.1f,                 // horizon_blur;
      {0.0, 0.0, 0.01f},    // night_color;
      0.8f,                 // sun_disk_intensity;
      {0.00, 0.78, 0.62f},  // sun_direction;
      5.0f,                 // sun_disk_scale;
      1.0f,                 // sun_glow_intensity;
      1,                    // y_is_up;
      1,                    // physically_scaled_sun;
      0,                    // in_use;
  };
}

void Scene::freeAllocData()
{
  m_pLightsAlloc->deinit(m_pContext);
  delete m_pLightsAlloc;
  m_pLightsAlloc = nullptr;

  m_pEnvMapAlloc->deinit(m_pContext);
  delete m_pEnvMapAlloc;
  m_pEnvMapAlloc = nullptr;

  // free textures alloc data
  for(auto& pTextureAlloc : m_pTexturesAlloc)
  {
    pTextureAlloc->deinit(m_pContext);
  }

  // free meshes alloc data
  for(auto& pMeshAlloc : m_pMeshesAlloc)
  {
    pMeshAlloc->deinit(m_pContext);
  }

  // free meshes alloc data
  for(auto& pMaterialAlloc : m_pMaterialsAlloc)
  {
    pMaterialAlloc->deinit(m_pContext);
  }

  // free scene desc alloc data
  m_pInstancesAlloc->deinit(m_pContext);
  delete m_pInstancesAlloc;
  m_pInstancesAlloc = nullptr;

  // free sun and sky
  m_pContext->getAlloc().destroy(m_bSunAndSky);
}

void Scene::freeRawData()
{
  //m_integrator = {};
  m_pipelineState = {};
  delete m_pCamera;
  m_pCamera = nullptr;
  delete m_pEnvMap;
  m_pEnvMap = nullptr;
  m_lights.clear();
  m_shots.clear();
  m_instances.clear();

  for(auto& record : m_pTextures)
  {
    const auto& valuePair = record.second;
    auto        pTexture  = valuePair.first;
    delete pTexture;
  }
  m_pTextures.clear();

  for(auto& record : m_pMeshes)
  {
    const auto& valuePair = record.second;
    auto        pMesh     = valuePair.first;
    delete pMesh;
  }
  m_pMeshes.clear();

  for(auto& record : m_pMaterials)
  {
    const auto& valuePair = record.second;
    auto        pMaterial = valuePair.first;
    delete pMaterial;
  }
  m_pMaterials.clear();
}

void Scene::addState(const State& piplineState)
{
  m_pipelineState = piplineState;
}

//void Scene::addIntegrator(int spp, int maxRecur, ToneMappingType tmType, uint useFaceNormal, uint ignoreEmissive, vec3 bgColor)
//{
//  m_integrator = Integrator(spp, maxRecur, tmType, useFaceNormal, ignoreEmissive, bgColor);
//}

void Scene::addCamera(VkExtent2D filmResolution, float fov, float focalDist, float aperture)
{
  if(m_pCamera)
    delete m_pCamera;
  m_pCamera = new CameraPerspective(filmResolution, fov, focalDist, aperture);
}

void Scene::addCamera(VkExtent2D filmResolution, vec4 fxfycxcy)
{
  if(m_pCamera)
    delete m_pCamera;
  m_pCamera = new CameraOpencv(filmResolution, fxfycxcy);
}

void Scene::addLight(const GpuLight& light)
{
  static char lightMeshName[30];
  Primitive   prim;
  int         lightId      = getLightsNum();
  bool        needMeshInst = false;
  if(light.type == LightTypeRect)
  {
    needMeshInst  = true;
    prim.type     = PrimitiveTypeRect;
    prim.position = light.position;
    prim.u        = light.u;
    prim.v        = light.v;
    sprintf(lightMeshName, "__rectLight:%d", lightId);
  }
  if(needMeshInst)
  {
    // add mesh
    Mesh* pMesh                           = new Mesh(prim);
    auto  meshId                          = m_pMeshes.size();
    m_pMeshes[std::string(lightMeshName)] = std::make_pair(pMesh, meshId);
    m_mesh2light[meshId]                  = std::make_pair(true, lightId);
    // add instance
    m_instances.emplace_back(Instance(meshId, lightId));
  }
  // add light
  m_lights.emplace_back(light);
}

void Scene::addEnvMap(const std::string& envmapPath)
{
  if(m_pEnvMap)
    delete m_pEnvMap;
  m_pEnvMap                                 = new EnvMap(envmapPath);
  auto size                                 = m_pEnvMap->getSize();
  m_pipelineState.rtxState.hasEnvMap        = 1;
  m_pipelineState.rtxState.envMapResolution = vec2(size.width, size.height);
}

void Scene::addTexture(const std::string& textureName, const std::string& texturePath, float gamma)
{
  Texture* pTexture        = new Texture(texturePath, gamma);
  m_pTextures[textureName] = std::make_pair(pTexture, m_pTextures.size());
}

void Scene::addMaterial(const std::string& materialName, const GpuMaterial& material)
{
  Material* pMaterial        = new Material(material);
  m_pMaterials[materialName] = std::make_pair(pMaterial, m_pMaterials.size());
}

void Scene::addMesh(const std::string& meshName, const std::string& meshPath, bool recomputeNormal)
{
  Mesh* pMesh         = new Mesh(meshPath, recomputeNormal);
  m_pMeshes[meshName] = std::make_pair(pMesh, m_pMeshes.size());
}

void Scene::addInstance(const nvmath::mat4f& transform, const std::string& meshName, const std::string& materialName)
{
  m_instances.emplace_back(Instance(transform, getMeshId(meshName), getMaterialId(materialName)));
}

void Scene::addShot(const CameraShot& shot)
{
  m_shots.emplace_back(shot);
}

//void Scene::addShot(const nvmath::vec3f& eye, const nvmath::vec3f& lookat, const nvmath::vec3f& up)
//{
//  m_shots.emplace_back(CameraShot{lookat, eye, up, nvmath::mat4f_zero});
//}
//
//void Scene::addShot(const mat4& ext)
//{
//  vec3 zero{0.f};
//  m_shots.emplace_back(CameraShot{zero, zero, zero, ext});
//}

int Scene::getMeshId(const std::string& meshName)
{
  if(m_pMeshes.count(meshName))
    return m_pMeshes[meshName].second;
  else
  {
    LOGE("[x] %-20s: mesh %s does not exist\n", "Scene Error", meshName.c_str());
    exit(1);
  }
  return 0;
}

int Scene::getTextureId(const std::string& textureName)
{
  if(m_pTextures.count(textureName))
    return m_pTextures[textureName].second;
  else
  {
    LOGE("[x] %-20s: texture %s does not exist\n", "Scene Error", textureName.c_str());
    exit(1);
  }
  return 0;
}

int Scene::getMaterialId(const std::string& materialName)
{
  if(m_pMaterials.count(materialName))
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
  return m_instances.size();
}

int Scene::getTexturesNum()
{
  return m_pTextures.size();
}

int Scene::getMaterialsNum()
{
  return m_pMaterials.size();
}

int Scene::getLightsNum()
{
  return m_lights.size();
}

int Scene::getShotsNum()
{
  return m_shots.size();
}

CameraShot& Scene::getShot(int shotId)
{
  return m_shots[shotId];
}

State& Scene::getPipelineState()
{
  return m_pipelineState;
}

/*
int Scene::getSpp()
{
  return m_integrator.getSpp();
}
*/

void Scene::setSpp(int spp)
{
  m_pipelineState.rtxState.spp = 1;
}

/*
int Scene::getMaxPathDepth()
{
  return m_integrator.getMaxPathDepth();
}

uint Scene::getUseFaceNormal()
{
  return m_integrator.getUseFaceNormal();
}
uint Scene::getToneMappingType()
{
  return m_integrator.getToneMappingType();
}
uint Scene::getIgnoreEmissive()
{
  return m_integrator.getIgnoreEmissive();
}

vec3 Scene::getBackGroundColor()
{
  return m_integrator.getBackgroundColor();
}
*/

Camera& Scene::getCamera()
{
  return *m_pCamera;
}

CameraType Scene::getCameraType()
{
  return m_pCamera->getType();
}

MaterialType Scene::getMaterialType(uint matId)
{
  return m_pMaterialsAlloc[matId]->getType();
}

nvvk::RaytracingBuilderKHR::BlasInput Scene::getBlas(VkDevice device, int meshId)
{
  return MeshBufferToBlas(device, *m_pMeshesAlloc[meshId]);
}

vector<Instance>& Scene::getInstances()
{
  return m_instances;
}

VkExtent2D Scene::getSize()
{
  return m_pCamera->getFilmSize();
}

VkBuffer Scene::getInstancesDescriptor()
{
  return m_pInstancesAlloc->getBuffer();
}

VkDescriptorImageInfo Scene::getTextureDescriptor(int textureId)
{
  return m_pTexturesAlloc[textureId]->getTexture();
}

vector<VkDescriptorImageInfo> Scene::getEnvMapDescriptor()
{
  return {m_pEnvMapAlloc->getEnvMap(), m_pEnvMapAlloc->getMarginal(), m_pEnvMapAlloc->getConditional()};
}

VkBuffer Scene::getLightsDescriptor()
{
  return m_pLightsAlloc->getBuffer();
}

VkBuffer Scene::getSunskyDescriptor()
{
  return m_bSunAndSky.buffer;
}

GpuSunAndSky& Scene::getSunsky()
{
  return m_sunAndSky;
}

void Scene::setShot(int shotId)
{
  m_pCamera->setToWorld(m_shots[shotId]);
  auto& state = m_shots[shotId].state;
  //m_pipelineState.rtxState.curFrame         = state.rtxState.curFrame;
  m_pipelineState.rtxState.spp          = state.rtxState.spp;
  m_pipelineState.rtxState.maxPathDepth = state.rtxState.maxPathDepth;
  //m_pipelineState.rtxState.numLights        = state.rtxState.numLights;
  m_pipelineState.rtxState.useFaceNormal  = state.rtxState.useFaceNormal;
  m_pipelineState.rtxState.ignoreEmissive = state.rtxState.ignoreEmissive;
  //m_pipelineState.rtxState.hasEnvMap        = state.rtxState.hasEnvMap;
  m_pipelineState.rtxState.envMapIntensity = state.rtxState.envMapIntensity;
  //m_pipelineState.rtxState.envMapResolution = state.rtxState.envMapResolution;
  m_pipelineState.rtxState.bgColor        = state.rtxState.bgColor;
  m_pipelineState.rtxState.envRotateAngle = state.rtxState.envRotateAngle;
}

void Scene::allocLights(ContextAware* pContext, const VkCommandBuffer& cmdBuf)
{
  m_pLightsAlloc = new LightsAlloc(pContext, m_lights, cmdBuf);
}

void Scene::allocTexture(ContextAware* pContext, uint32_t textureId, const std::string& textureName, Texture* pTexture, const VkCommandBuffer& cmdBuf)
{
  auto& m_debug  = pContext->getDebug();
  auto  m_device = pContext->getDevice();

  TextureAlloc* pTextureAlloc = new TextureAlloc(pContext, pTexture, cmdBuf);
  m_pTexturesAlloc[textureId] = pTextureAlloc;
}

void Scene::allocMaterial(ContextAware* pContext, uint32_t materialId, const std::string& materialName, Material* pMaterial, const VkCommandBuffer& cmdBuf)
{
  auto& m_debug  = pContext->getDebug();
  auto  m_device = pContext->getDevice();

  MaterialAlloc* pMaterialAlloc = new MaterialAlloc(pContext, pMaterial, cmdBuf);
  m_pMaterialsAlloc[materialId] = pMaterialAlloc;

  m_debug.setObjectName(pMaterialAlloc->getBuffer(), std::string(materialName + "_materialBuffer"));
}

void Scene::allocMesh(ContextAware* pContext, uint32_t meshId, const std::string& meshName, Mesh* pMesh, const VkCommandBuffer& cmdBuf)
{
  auto& m_debug  = pContext->getDebug();
  auto  m_device = pContext->getDevice();

  MeshAlloc* pMeshAlloc  = new MeshAlloc(pContext, pMesh, cmdBuf);
  m_pMeshesAlloc[meshId] = pMeshAlloc;

  NAME2_VK(pMeshAlloc->getVerticesBuffer(), std::string(meshName + "_vertexBuffer"));
  NAME2_VK(pMeshAlloc->getIndicesBuffer(), std::string(meshName + "_indexBuffer"));
}

void Scene::allocInstances(ContextAware* pContext, const VkCommandBuffer& cmdBuf)
{
  // Keeping the obj host model and device description
  m_pInstancesAlloc = new InstancesAlloc(pContext, m_instances, m_pMeshesAlloc, m_pMaterialsAlloc, cmdBuf);
}

void Scene::allocEnvMap(ContextAware* pContext, const VkCommandBuffer& cmdBuf)
{
  m_pEnvMapAlloc = new EnvMapAlloc(pContext, m_pEnvMap, cmdBuf);
}

void Scene::allocSunAndSky(ContextAware* pContext, const VkCommandBuffer& cmdBuf)
{
  auto& m_alloc = pContext->getAlloc();
  auto& m_debug = pContext->getDebug();
  m_bSunAndSky = m_alloc.createBuffer(sizeof(GpuSunAndSky), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  NAME_VK(m_bSunAndSky.buffer);
}

void Scene::computeSceneDimensions()
{
  Bbox scnBbox;

  for(auto& inst : m_instances)
  {
    auto pMeshAlloc = m_pMeshesAlloc[inst.getMeshIndex()];
    Bbox bbox(pMeshAlloc->getPosMin(), pMeshAlloc->getPosMax());
    bbox.transform(inst.getTransform());
    scnBbox.insert(bbox);
  }

  if(scnBbox.isEmpty() || !scnBbox.isVolume())
  {
    LOGE("[!] %-20s: Scene bounding box invalid, Setting to: [-1,-1,-1], [1,1,1]", "Scene Warning");
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
  auto m_size = getSize();
  CameraManip.fit(m_dimensions.min, m_dimensions.max, true, false, m_size.width / static_cast<float>(m_size.height));
  auto cam = CameraManip.getCamera();
  m_shots.emplace_back(CameraShot{cam.ctr, cam.eye, cam.up, nvmath::mat4f_zero, getPipelineState()});
}
