#include "loader.h"
#include "utils.h"
#include <shared/camera.h>
#include <shared/pushconstant.h>

#include <nvmath/nvmath.h>
#include <nvh/fileoperations.hpp>
#include <nvh/timesampler.hpp>
#include <nvvk/buffers_vk.hpp>
#include <nvvk/commands_vk.hpp>

#include <filesystem>
#include <fstream>

#define PI 3.14159265358979323846f

using nlohmann::json;
using std::ifstream;
using std::string;
using std::filesystem::path;

static json defaultSceneOptions = json::parse(R"(
{
  "integrator": {
    "spp": 1,
    "max_path_depth": 2,
    "tone_mapping": "filmic",
    "use_face_normal": false,
    "ignore_emissive": false,
    "background_color": [0,0,0]
  },
  "camera": {
    "fov": 45.0,
    "aperture": 0.0,
    "focal_distance": 0.1
  },
  "textures": {
    "gamma": 1.0
  },
  "materials": {
  }
}
)");

Loader::Loader(Scene* pScene)
    : m_pScene(pScene)
{
}

void Loader::loadSceneFromJson(std::string sceneFilePath, const std::vector<std::string>& root)
{
  nvh::Stopwatch sw_;
  bool           isRelativePath = path(sceneFilePath).is_relative();
  if(isRelativePath)
    sceneFilePath = nvh::findFile(sceneFilePath, root, true);
  if(sceneFilePath.empty())
    exit(1);
  m_sceneFileDir = path(sceneFilePath).parent_path().string();

  ifstream sceneFileStream(sceneFilePath);
  json     sceneFileJson;
  sceneFileStream >> sceneFileJson;

  m_pScene->reset();
  parse(sceneFileJson);
  submit();
}

void Loader::parse(const nlohmann::json& sceneFileJson)
{
  JsonCheckKeys(sceneFileJson, {"integrator", "camera", "meshes", "instances"});

  auto& integratorJson = sceneFileJson["integrator"];
  auto& cameraJson     = sceneFileJson["camera"];
  auto& meshesJson     = sceneFileJson["meshes"];
  auto& instancesJson  = sceneFileJson["instances"];

  // parse scene file to generate raw data
  // integrator
  addIntegrator(integratorJson);
  // camera
  addCamera(cameraJson);
  // textures
  if(sceneFileJson.contains("textures"))
  {
    auto& texturesJson = sceneFileJson["textures"];
    for(auto& textureJson : texturesJson)
    {
      addTexture(textureJson);
    }
  }
  // materials
  if(sceneFileJson.contains("materials"))
  {
    auto& materialsJson = sceneFileJson["materials"];
    for(auto& materialJson : materialsJson)
    {
      addMaterial(materialJson);
    }
  }
  // lights
  if(sceneFileJson.contains("lights"))
  {
    auto& lightsJson = sceneFileJson["lights"];
    for(auto& lightJson : lightsJson)
    {
      addLight(lightJson);
    }
  }
  // meshes
  for(auto& meshJson : meshesJson)
  {
    addMesh(meshJson);
  }
  // instances
  for(auto& instanceJson : instancesJson)
  {
    addInstance(instanceJson);
  }
  // shots
  if(sceneFileJson.contains("shots"))
  {
    auto& shotsJson = sceneFileJson["shots"];
    for(auto& shotJson : shotsJson)
    {
      addShot(shotJson);
    }
  }
}

void Loader::submit()
{
  m_pScene->submit();
}

void Loader::addIntegrator(const nlohmann::json& integratorJson)
{
  int    spp                = defaultSceneOptions["integrator"]["spp"];
  int    maxDepth           = defaultSceneOptions["integrator"]["max_path_depth"];
  string strToneMapping     = defaultSceneOptions["integrator"]["tone_mapping"];
  bool   boolUseFaceNormal  = defaultSceneOptions["integrator"]["use_face_normal"];
  bool   boolIgnoreEmissive = defaultSceneOptions["integrator"]["ignore_emissive"];
  vec3   bgColor            = Json2Vec3(defaultSceneOptions["integrator"]["background_color"]);
  if(integratorJson.contains("spp"))
    spp = integratorJson["spp"];
  if(integratorJson.contains("max_path_depth"))
    maxDepth = integratorJson["max_path_depth"];
  if(integratorJson.contains("tone_mapping"))
    strToneMapping = integratorJson["tone_mapping"];
  if(integratorJson.contains("use_face_normal"))
    boolUseFaceNormal = integratorJson["use_face_normal"];
  if(integratorJson.contains("ignore_emissive"))
    boolIgnoreEmissive = integratorJson["ignore_emissive"];
  if(integratorJson.contains("background_color"))
    bgColor = Json2Vec3(integratorJson["background_color"]);

  ToneMappingType tmType = ToneMappingTypeNone;
  if(strToneMapping == "none")
    tmType = ToneMappingTypeNone;
  else if(strToneMapping == "gamma")
    tmType = ToneMappingTypeGamma;
  else if(strToneMapping == "reinhard")
    tmType = ToneMappingTypeReinhard;
  else if(strToneMapping == "Aces")
    tmType = ToneMappingTypeAces;
  else if(strToneMapping == "filmic")
    tmType = ToneMappingTypeFilmic;
  else if(strToneMapping == "pbrt")
    tmType = ToneMappingTypePbrt;
  else if(strToneMapping == "custom")
    tmType = ToneMappingTypeCustom;

  m_pScene->addIntegrator(spp, maxDepth, tmType, boolUseFaceNormal, boolIgnoreEmissive, bgColor);
}

void Loader::addCamera(const nlohmann::json& cameraJson)
{
  JsonCheckKeys(cameraJson, {"type", "film"});
  auto& filmJson = cameraJson["film"];
  JsonCheckKeys(filmJson, {"resolution"});
  vec2          resolution     = Json2Vec2(filmJson["resolution"]);
  VkExtent2D    filmResolution = {uint(resolution.x), uint(resolution.y)};
  nvmath::vec4f fxfycxcy       = 0.0f;
  float         fov            = defaultSceneOptions["camera"]["fov"];
  float         aperture       = defaultSceneOptions["camera"]["aperture"];
  float         focalDistance  = defaultSceneOptions["camera"]["focal_distance"];
  if(cameraJson["type"] == "perspective")
  {
    if(cameraJson.contains("fov"))
      fov = cameraJson["fov"];
    if(cameraJson.contains("aperture"))
      aperture = cameraJson["aperture"];
    if(cameraJson.contains("focal_distance"))
      focalDistance = cameraJson["focal_distance"];
    m_pScene->addCamera(filmResolution, fov, focalDistance, aperture);
  }
  else if(cameraJson["type"] == "opencv")
  {
    JsonCheckKeys(cameraJson, {"fx", "fy", "cx", "cy"});
    fxfycxcy = {cameraJson["fx"], cameraJson["fy"], cameraJson["cx"], cameraJson["cy"]};
    m_pScene->addCamera(filmResolution, fxfycxcy);
  }
  else
  {
    // TODO
    exit(1);
  }
}

void Loader::addLight(const nlohmann::json& lightJson)
{
  JsonCheckKeys(lightJson, {"type", "emittance"});
  GpuLight light = {
      LightTypeUndefined,  // type
      vec3(0.0),           // position
      vec3(0.0),           // direction
      vec3(0.0),           // emittance
      vec3(0.0),           // u
      vec3(0.0),           // v
      0.0,                 // radius
      0.0,                 // area
      0,                   // double side
  };
  light.emittance = Json2Vec3(lightJson["emittance"]);
  if(lightJson["type"] == "rect")
  {
    JsonCheckKeys(lightJson, {"position", "v1", "v2"});
    vec3 v1        = Json2Vec3(lightJson["v1"]);
    vec3 v2        = Json2Vec3(lightJson["v2"]);
    light.position = Json2Vec3(lightJson["position"]);
    light.u        = v1 - light.position;
    light.v        = v2 - light.position;
    light.area     = nvmath::length(nvmath::cross(light.u, light.v));
    light.type     = LightTypeRect;
    if(lightJson.contains("double_side"))
      light.doubleSide = lightJson["double_side"] ? 1 : 0;
  }
  //   else if(lightJson["type"] == "sphere")
  //   {
  //     JsonCheckKeys(lightJson, {"position", "radius"});
  //     light.position = Json2Vec3(lightJson["position"]);
  //     light.radius   = lightJson["radius"];
  //     light.area     = 4.0f * PI * light.radius * light.radius;
  //     light.type     = GpuLightType::eSphereLight;
  //   }
  //else if (lightJson["type"] == "point")
  //{
  //    JsonCheckKeys(lightJson, {"position", "radius"});
  //    light.position = Json2Vec3(lightJson["position"]);
  //    light.radius   = lightJson["radius"];
  //    light.area     = 4.0f * PI * light.radius * light.radius;
  //    light.type     = GpuLightType::eSphereLight;
  //}
  else if(lightJson["type"] == "distant")
  {
    JsonCheckKeys(lightJson, {"direction"});
    light.direction = Json2Vec3(lightJson["direction"]);
    light.type      = LightTypeDirectional;
  }
  else
  {
    // TODO
    exit(1);
  }

  m_pScene->addLight(light);
}

void Loader::addTexture(const nlohmann::json& textureJson)
{
  JsonCheckKeys(textureJson, {"name", "path"});
  std::string textureName = textureJson["name"];
  auto        texturePath = nvh::findFile(textureJson["path"], {m_sceneFileDir}, true);
  if(texturePath.empty())
  {
    LOGE("[x] %-20s: failed to load texture from %s\n", "Scene Error", std::string(textureJson["path"]).c_str());
    exit(1);
  }
  float gamma = defaultSceneOptions["textures"]["gamma"];
  if(textureJson.contains("gamma"))
    gamma = textureJson["gamma"];

  m_pScene->addTexture(textureName, texturePath, gamma);
}

void Loader::addMaterial(const nlohmann::json& materialJson)
{
  JsonCheckKeys(materialJson, {"type", "name"});
  std::string  materialName = materialJson["name"];
  Material     mat;
  GpuMaterial& material = mat.getMaterial();
  if(materialJson["type"] == "brdf_lambertian")
  {
    material.type = MaterialTypeBrdfLambertian;
    if(materialJson.contains("diffuse_reflectance"))
      material.diffuse = Json2Vec3(materialJson["diffuse_reflectance"]);
    if(materialJson.contains("diffuse_texture"))
      material.diffuseTextureId = m_pScene->getTextureId(materialJson["diffuse_texture"]);
    if(materialJson.contains("emittance"))
      material.emittance = Json2Vec3(materialJson["emittance"]);
    if(materialJson.contains("emittance_factor"))
      material.emittanceFactor = Json2Vec3(materialJson["emittance_factor"]);
    if(materialJson.contains("emittance_texture"))
      material.emittanceTextureId = m_pScene->getTextureId(materialJson["emittance_texture"]);
  }
  else if(materialJson["type"] == "brdf_pbr_metalness_roughness")
  {
    material.type = MaterialTypeBrdfPbrMetalnessRoughness;
    if(materialJson.contains("diffuse_reflectance"))
      material.diffuse = Json2Vec3(materialJson["diffuse_reflectance"]);
    if(materialJson.contains("diffuse_texture"))
      material.diffuseTextureId = m_pScene->getTextureId(materialJson["diffuse_texture"]);
    if(materialJson.contains("emittance"))
      material.emittance = Json2Vec3(materialJson["emittance"]);
    if(materialJson.contains("emittance_factor"))
      material.emittanceFactor = Json2Vec3(materialJson["emittance_factor"]);
    if(materialJson.contains("emittance_texture"))
      material.emittanceTextureId = m_pScene->getTextureId(materialJson["emittance_texture"]);
    if(materialJson.contains("metalness"))
      material.metalness = materialJson["metalness"];
    if(materialJson.contains("metalness_texture"))
      material.metalnessTextureId = m_pScene->getTextureId(materialJson["metalness_texture"]);
  }
  else
  {
    // TODO
    exit(1);
  }

  m_pScene->addMaterial(materialName, material);
}

void Loader::addMesh(const nlohmann::json& meshJson)
{
  JsonCheckKeys(meshJson, {"name", "path"});
  std::string meshName = meshJson["name"];
  auto        meshPath = nvh::findFile(meshJson["path"], {m_sceneFileDir}, true);
  if(meshPath.empty())
  {
    // TODO: LOGE
    exit(1);
  }
  bool recomputeNormal = false;
  if(meshJson.contains("recompute_normal"))
    recomputeNormal = meshJson["recompute_normal"];

  m_pScene->addMesh(meshName, meshPath, recomputeNormal);
}

void Loader::addInstance(const nlohmann::json& instanceJson)
{
  JsonCheckKeys(instanceJson, {"mesh"});
  string meshName = instanceJson["mesh"], materialName;
  if(instanceJson.count("material"))
    materialName = instanceJson["material"];
  else
    materialName = defaultSceneOptions["materials"]["name"];
  nvmath::mat4f transform = nvmath::mat4f_id;
  if(instanceJson.contains("toworld"))
  {
    for(const auto& singleton : instanceJson["toworld"])
    {
      nvmath::mat4f t = nvmath::mat4f_id;
      JsonCheckKeys(singleton, {"type", "value"});
      if(singleton["type"] == "matrix")
      {
        t = Json2Mat4(singleton["value"]);
      }
      else if(singleton["type"] == "translate")
      {
        t.set_translation(Json2Vec3(singleton["value"]));
      }
      else if(singleton["type"] == "scale")
      {
        t.set_scale(Json2Vec3(singleton["scale"]));
      }
      else if(singleton["type"] == "rotx")
      {
        t = nvmath::rotation_mat4_x(float(singleton["value"]));
      }
      else if(singleton["type"] == "roty")
      {
        t = nvmath::rotation_mat4_y(float(singleton["value"]));
      }
      else if(singleton["type"] == "rotz")
      {
        t = nvmath::rotation_mat4_z(float(singleton["value"]));
      }
      else
      {
        // TODO
        exit(1);
      }
      transform = t * transform;
    }
  }

  m_pScene->addInstance(transform, meshName, materialName);
}

void Loader::addShot(const nlohmann::json& shotJson)
{
  JsonCheckKeys(shotJson, {"type"});
  if(shotJson["type"] == "lookat")
  {
    JsonCheckKeys(shotJson, {"eye", "lookat", "up"});
    m_pScene->addShot(Json2Vec3(shotJson["eye"]), Json2Vec3(shotJson["lookat"]), Json2Vec3(shotJson["up"]));
  }
  else if(shotJson["type"] == "opencv")
  {
    JsonCheckKeys(shotJson, {"matrix", "up"});
    mat4 ext = Json2Mat4(shotJson["matrix"]);
    //ext.a00 = -ext.a00, ext.a01 = -ext.a01, ext.a02 = -ext.a02, ext.a03 = -ext.a03;
    //ext.a20 = -ext.a20, ext.a21 = -ext.a21, ext.a22 = -ext.a22, ext.a23 = -ext.a23;
    m_pScene->addShot(ext);
  }
}
