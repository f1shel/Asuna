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
  "camera": {
    "fov": 45.0,
    "aperture": 0.0,
    "focal_distance": 0.1
  },
  "textures": {
    "gamma": 1.0
  },
  "materials": {
  },
  "envmap": {
    "intensity": 1.0
  }
}
)");

VkExtent2D Loader::loadSizeFirst(std::string sceneFilePath,
                                 const std::string& root) {
  bool isRelativePath = path(sceneFilePath).is_relative();
  if (isRelativePath)
    sceneFilePath = nvh::findFile(sceneFilePath, {root}, true);
  if (sceneFilePath.empty()) {
    LOG_ERROR("{}: failed to load scene from file [{}]", "Loader",
              sceneFilePath);
    exit(1);
  }
  m_sceneFileDir = path(sceneFilePath).parent_path().string();

  ifstream sceneFileStream(sceneFilePath);
  json sceneFileJson;
  sceneFileStream >> sceneFileJson;

  JsonCheckKeys(sceneFileJson, {"state", "camera", "meshes", "instances"});
  auto& cameraJson = sceneFileJson["camera"];
  JsonCheckKeys(cameraJson, {"type", "film"});
  auto& filmJson = cameraJson["film"];
  JsonCheckKeys(filmJson, {"resolution"});
  vec2 resolution = Json2Vec2(filmJson["resolution"]);
  VkExtent2D filmResolution = {uint(resolution.x), uint(resolution.y)};

  return filmResolution;
}

void Loader::loadSceneFromJson(std::string sceneFilePath,
                               const std::string& root, Scene* pScene) {
  LOG_INFO("{}: loading scene assets, this may take tens of seconds", "Loader");

  bool isRelativePath = path(sceneFilePath).is_relative();
  if (isRelativePath)
    sceneFilePath = nvh::findFile(sceneFilePath, {root}, true);
  if (sceneFilePath.empty()) {
    LOG_ERROR("{}: failed to load scene from file [{}]", "Loader",
              sceneFilePath);
    exit(1);
  }
  m_sceneFileDir = path(sceneFilePath).parent_path().string();

  ifstream sceneFileStream(sceneFilePath);
  json sceneFileJson;
  sceneFileStream >> sceneFileJson;

  m_pScene = pScene;
  m_pScene->reset();
  parse(sceneFileJson);
  submit();
}

void Loader::parse(const nlohmann::json& sceneFileJson) {
  JsonCheckKeys(sceneFileJson, {"state", "camera", "meshes", "instances"});

  auto& stateJson = sceneFileJson["state"];
  auto& cameraJson = sceneFileJson["camera"];
  auto& meshesJson = sceneFileJson["meshes"];
  auto& instancesJson = sceneFileJson["instances"];

  // parse scene file to generate raw data
  addState(stateJson);
  // camera
  addCamera(cameraJson);
  // textures
  if (sceneFileJson.contains("textures")) {
    auto& texturesJson = sceneFileJson["textures"];
    for (auto& textureJson : texturesJson) {
      addTexture(textureJson);
    }
  }
  // materials
  if (sceneFileJson.contains("materials")) {
    auto& materialsJson = sceneFileJson["materials"];
    for (auto& materialJson : materialsJson) {
      addMaterial(materialJson);
    }
  }
  // lights
  if (sceneFileJson.contains("lights")) {
    auto& lightsJson = sceneFileJson["lights"];
    for (auto& lightJson : lightsJson) {
      addLight(lightJson);
    }
  }
  // env map
  if (sceneFileJson.contains("envmap")) {
    auto& envmapJson = sceneFileJson["envmap"];
    addEnvMap(envmapJson);
  }
  // meshes
  for (auto& meshJson : meshesJson) {
    addMesh(meshJson);
  }
  // instances
  for (auto& instanceJson : instancesJson) {
    addInstance(instanceJson);
  }
  // shots
  if (sceneFileJson.contains("shots")) {
    auto& shotsJson = sceneFileJson["shots"];
    for (auto& shotJson : shotsJson) {
      addShot(shotJson);
    }
  }
}

void Loader::submit() { m_pScene->submit(); }

static void parseState(const nlohmann::json& stateJson, State& pipelineState) {
  if (stateJson.contains("path_tracing")) {
    const auto& ptJson = stateJson["path_tracing"];
    auto& rtxState = pipelineState.rtxState;
    if (ptJson.contains("spp")) rtxState.spp = ptJson["spp"];
    if (ptJson.contains("max_path_depth"))
      rtxState.maxPathDepth = ptJson["max_path_depth"];
    if (ptJson.contains("use_face_normal"))
      rtxState.useFaceNormal = ptJson["use_face_normal"] ? 1.f : 0.f;
    if (ptJson.contains("ignore_emissive"))
      rtxState.ignoreEmissive = ptJson["ignore_emissive"] ? 1.f : 0.f;
    if (ptJson.contains("background_color"))
      rtxState.bgColor = Json2Vec3(ptJson["background_color"]);
    if (ptJson.contains("envmap_intensity"))
      rtxState.envMapIntensity = ptJson["envmap_intensity"];
    if (ptJson.contains("envmap_rotate"))
      rtxState.envRotateAngle = ptJson["envmap_rotate"];
  }

  if (stateJson.contains("post_processing")) {
    const auto& postJson = stateJson["post_processing"];
    auto& postState = pipelineState.postState;
    string strToneMapping = "";
    if (postJson.contains("tone_mapping"))
      strToneMapping = postJson["tone_mapping"];

    ToneMappingType tmType = ToneMappingTypeNone;
    if (strToneMapping == "none")
      tmType = ToneMappingTypeNone;
    else if (strToneMapping == "gamma")
      tmType = ToneMappingTypeGamma;
    else if (strToneMapping == "reinhard")
      tmType = ToneMappingTypeReinhard;
    else if (strToneMapping == "Aces")
      tmType = ToneMappingTypeAces;
    else if (strToneMapping == "filmic")
      tmType = ToneMappingTypeFilmic;
    else if (strToneMapping == "pbrt")
      tmType = ToneMappingTypePbrt;
    else if (strToneMapping == "custom")
      tmType = ToneMappingTypeCustom;
    else
      LOG_WARN(
          "{}: no matching tone mapper for [{}], use default"
          "Loader",
          strToneMapping);

    postState.tmType = tmType;
  }
}

void Loader::addState(const nlohmann::json& stateJson) {
  State pipelineState;
  parseState(stateJson, pipelineState);
  m_pScene->addState(pipelineState);
}

void Loader::addCamera(const nlohmann::json& cameraJson) {
  JsonCheckKeys(cameraJson, {"type", "film"});
  auto& filmJson = cameraJson["film"];
  JsonCheckKeys(filmJson, {"resolution"});
  vec2 resolution = Json2Vec2(filmJson["resolution"]);
  VkExtent2D filmResolution = {uint(resolution.x), uint(resolution.y)};
  nvmath::vec4f fxfycxcy = 0.0f;
  float fov = defaultSceneOptions["camera"]["fov"];
  float aperture = defaultSceneOptions["camera"]["aperture"];
  float focalDistance = defaultSceneOptions["camera"]["focal_distance"];
  if (cameraJson["type"] == "perspective") {
    if (cameraJson.contains("fov")) fov = cameraJson["fov"];
    if (cameraJson.contains("aperture")) aperture = cameraJson["aperture"];
    if (cameraJson.contains("focal_distance"))
      focalDistance = cameraJson["focal_distance"];
    m_pScene->addCamera(filmResolution, fov, focalDistance, aperture);
  } else if (cameraJson["type"] == "opencv") {
    JsonCheckKeys(cameraJson, {"fx", "fy", "cx", "cy"});
    fxfycxcy = {cameraJson["fx"], cameraJson["fy"], cameraJson["cx"],
                cameraJson["cy"]};
    m_pScene->addCamera(filmResolution, fxfycxcy);
  } else {
    LOG_ERROR("{}: unrecognized camera type [{}]", "Loader",
              cameraJson["type"]);
    exit(1);
  }
}

void Loader::addLight(const nlohmann::json& lightJson) {
  JsonCheckKeys(lightJson, {"type", "radiance"});
  GpuLight light = {
      LightTypeUndefined,  // type
      vec3(0.0),           // position
      vec3(0.0),           // direction
      vec3(0.0),           // radiance
      vec3(0.0),           // u
      vec3(0.0),           // v
      0.0,                 // radius
      0.0,                 // area
      0,                   // double side
  };
  light.radiance = Json2Vec3(lightJson["radiance"]);
  if (lightJson["type"] == "rect" || lightJson["type"] == "triangle") {
    JsonCheckKeys(lightJson, {"position", "v1", "v2"});
    vec3 v1 = Json2Vec3(lightJson["v1"]);
    vec3 v2 = Json2Vec3(lightJson["v2"]);
    light.position = Json2Vec3(lightJson["position"]);
    light.u = v1 - light.position;
    light.v = v2 - light.position;
    light.area = nvmath::length(nvmath::cross(light.u, light.v));
    light.type = LightTypeRect;
    if (lightJson.contains("double_side"))
      light.doubleSide = lightJson["double_side"] ? 1 : 0;
    if (lightJson["type"] == "triangle") light.area *= 0.5f;
  }
  //   else if(lightJson["type"] == "sphere")
  //   {
  //     JsonCheckKeys(lightJson, {"position", "radius"});
  //     light.position = Json2Vec3(lightJson["position"]);
  //     light.radius   = lightJson["radius"];
  //     light.area     = 4.0f * PI * light.radius * light.radius;
  //     light.type     = GpuLightType::eSphereLight;
  //   }
  // else if (lightJson["type"] == "point")
  //{
  //    JsonCheckKeys(lightJson, {"position", "radius"});
  //    light.position = Json2Vec3(lightJson["position"]);
  //    light.radius   = lightJson["radius"];
  //    light.area     = 4.0f * PI * light.radius * light.radius;
  //    light.type     = GpuLightType::eSphereLight;
  //}
  else if (lightJson["type"] == "distant") {
    JsonCheckKeys(lightJson, {"direction"});
    light.direction = Json2Vec3(lightJson["direction"]);
    light.type = LightTypeDirectional;
  } else if (lightJson["type"] == "mesh") {
    JsonCheckKeys(lightJson, {"path"});
    string meshPath = lightJson["path"];
    meshPath = nvh::findFile(meshPath, {m_sceneFileDir});
    if (meshPath.empty()) {
      LOG_ERROR("{}: failed to load mesh light from [{}]", "Loader",
                lightJson["path"]);
      exit(1);
    }
    light.type = LightTypeTriangle;
    m_pScene->addLight(light, meshPath);
    return;
  } else {
    LOG_ERROR("{}: unrecognized light type [{}]", "Loader", lightJson["type"]);
    exit(1);
  }

  m_pScene->addLight(light);
}

void Loader::addTexture(const nlohmann::json& textureJson) {
  JsonCheckKeys(textureJson, {"name", "path"});
  std::string textureName = textureJson["name"];
  auto texturePath = nvh::findFile(textureJson["path"], {m_sceneFileDir}, true);
  if (texturePath.empty()) {
    LOG_ERROR("{}: failed to load texture from file [{%s}]", "Loader",
              textureJson["path"]);
    exit(1);
  }
  float gamma = defaultSceneOptions["textures"]["gamma"];
  if (textureJson.contains("gamma")) gamma = textureJson["gamma"];

  m_pScene->addTexture(textureName, texturePath, gamma);
}

static inline float dielectricReflectance(float eta, float cosThetaI,
                                          float& cosThetaT) {
  if (cosThetaI < 0.0f) {
    eta = 1.0f / eta;
    cosThetaI = -cosThetaI;
  }
  float sinThetaTSq = eta * eta * (1.0f - cosThetaI * cosThetaI);
  if (sinThetaTSq > 1.0f) {
    cosThetaT = 0.0f;
    return 1.0f;
  }
  cosThetaT = std::sqrt(std::max(1.0f - sinThetaTSq, 0.0f));

  float Rs = (eta * cosThetaI - cosThetaT) / (eta * cosThetaI + cosThetaT);
  float Rp = (eta * cosThetaT - cosThetaI) / (eta * cosThetaT + cosThetaI);

  return (Rs * Rs + Rp * Rp) * 0.5f;
}

static inline float computeDiffuseFresnel(float ior, const int sampleCount) {
  double diffuseFresnel = 0.0;
  float _, fb = dielectricReflectance(ior, 0.0f, _);
  for (int i = 1; i <= sampleCount; ++i) {
    float cosThetaSq = float(i) / sampleCount;
    float _, fa = dielectricReflectance(
                 ior, std::min(std::sqrt(cosThetaSq), 1.0f), _);
    diffuseFresnel += double(fa + fb) * (0.5 / sampleCount);
    fb = fa;
  }

  return float(diffuseFresnel);
}

void Loader::addMaterial(const nlohmann::json& materialJson) {
  JsonCheckKeys(materialJson, {"type", "name"});
  std::string materialName = materialJson["name"];
  Material mat;
  GpuMaterial& material = mat.getMaterial();
  if (materialJson["type"] == "brdf_lambertian") {
    material.type = MaterialTypeBrdfLambertian;
    if (materialJson.contains("diffuse_reflectance"))
      material.diffuse = Json2Vec3(materialJson["diffuse_reflectance"]);
    if (materialJson.contains("diffuse_texture"))
      material.diffuseTextureId =
          m_pScene->getTextureId(materialJson["diffuse_texture"]);
    if (materialJson.contains("normal_texture"))
      material.normalTextureId =
          m_pScene->getTextureId(materialJson["normal_texture"]);
  } else if (materialJson["type"] == "brdf_pbr_metalness_roughness") {
    material.type = MaterialTypeBrdfPbrMetalnessRoughness;
    if (materialJson.contains("normal_texture"))
      material.normalTextureId =
          m_pScene->getTextureId(materialJson["normal_texture"]);
    if (materialJson.contains("diffuse_reflectance"))
      material.diffuse = Json2Vec3(materialJson["diffuse_reflectance"]);
    if (materialJson.contains("diffuse_texture"))
      material.diffuseTextureId =
          m_pScene->getTextureId(materialJson["diffuse_texture"]);
    if (materialJson.contains("metalness"))
      material.metalness = materialJson["metalness"];
    if (materialJson.contains("metalness_texture"))
      material.metalnessTextureId =
          m_pScene->getTextureId(materialJson["metalness_texture"]);
    if (materialJson.contains("roughness"))
      material.roughness = materialJson["roughness"];
    if (materialJson.contains("roughness_texture"))
      material.roughnessTextureId =
          m_pScene->getTextureId(materialJson["roughness_texture"]);
  } else if (materialJson["type"] == "brdf_emissive") {
    material.type = MaterialTypeBrdfEmissive;
    if (materialJson.contains("radiance"))
      material.radiance = Json2Vec3(materialJson["radiance"]);
    if (materialJson.contains("radiance_factor"))
      material.radianceFactor = Json2Vec3(materialJson["radiance_factor"]);
    if (materialJson.contains("radiance_texture"))
      material.radianceTextureId =
          m_pScene->getTextureId(materialJson["radiance_texture"]);
  } else if (materialJson["type"] == "brdf_kang18") {
    material.type = MaterialTypeBrdfKang18;
    JsonCheckKeys(materialJson,
                  {"diffuse_texture", "specular_texture", "normal_texture",
                   "alpha_texture", "tangent_texture"});
    material.normalTextureId =
        m_pScene->getTextureId(materialJson["normal_texture"]);
    material.diffuseTextureId =
        m_pScene->getTextureId(materialJson["diffuse_texture"]);
    material.metalnessTextureId =
        m_pScene->getTextureId(materialJson["specular_texture"]);
    material.roughnessTextureId =
        m_pScene->getTextureId(materialJson["alpha_texture"]);
    material.tangentTextureId =
        m_pScene->getTextureId(materialJson["tangent_texture"]);
  } else if (materialJson["type"] == "bsdf_dielectric") {
    material.type = MaterialTypeBsdfDielectric;
    if (materialJson.contains("normal_texture"))
      material.normalTextureId =
          m_pScene->getTextureId(materialJson["normal_texture"]);
    if (materialJson.contains("ior")) material.ior = materialJson["ior"];
  } else if (materialJson["type"] == "brdf_plastic") {
    material.type = MaterialTypeBrdfPlastic;
    if (materialJson.contains("ior")) material.ior = materialJson["ior"];
    if (materialJson.contains("diffuse_reflectance"))
      material.diffuse = Json2Vec3(materialJson["diffuse_reflectance"]);
    if (materialJson.contains("diffuse_texture"))
      material.diffuseTextureId =
          m_pScene->getTextureId(materialJson["diffuse_texture"]);
    if (materialJson.contains("normal_texture"))
      material.normalTextureId =
          m_pScene->getTextureId(materialJson["normal_texture"]);
    // fresnel diffuse reflectance, AKA fdrInt
    material.radiance.x = computeDiffuseFresnel(material.ior, 1000);
  } else {
    LOG_ERROR("{}: unrecognized material type [{}]", "Loader",
              materialJson["type"]);
    exit(1);
  }

  m_pScene->addMaterial(materialName, material);
}

void Loader::addMesh(const nlohmann::json& meshJson) {
  JsonCheckKeys(meshJson, {"name", "path"});
  std::string meshName = meshJson["name"];
  auto meshPath = nvh::findFile(meshJson["path"], {m_sceneFileDir}, true);
  if (meshPath.empty()) {
    LOG_ERROR("{}: failed to load mesh from file [{}]", "Loader", meshPath);
    exit(1);
  }
  bool recomputeNormal = false;
  vec2 uvScale = {1.f, 1.f};
  if (meshJson.contains("recompute_normal"))
    recomputeNormal = meshJson["recompute_normal"];
  if (meshJson.contains("uv_scale")) uvScale = Json2Vec2(meshJson["uv_scale"]);

  m_pScene->addMesh(meshName, meshPath, recomputeNormal, uvScale);
}

static void parseToWorld(const json& toworldJson, mat4& transform,
                         bool banTranslation = false) {
  transform = nvmath::mat4f_id;
  for (const auto& singleton : toworldJson) {
    nvmath::mat4f t = nvmath::mat4f_id;
    JsonCheckKeys(singleton, {"type", "value"});
    if (singleton["type"] == "matrix") {
      t = Json2Mat4(singleton["value"]);
    } else if (singleton["type"] == "translate" && !banTranslation) {
      t.set_translation(Json2Vec3(singleton["value"]));
    } else if (singleton["type"] == "scale") {
      t.set_scale(Json2Vec3(singleton["value"]));
    } else if (singleton["type"] == "rotx") {
      t = nvmath::rotation_mat4_x(nv_to_rad * float(singleton["value"]));
    } else if (singleton["type"] == "roty") {
      t = nvmath::rotation_mat4_y(nv_to_rad * float(singleton["value"]));
    } else if (singleton["type"] == "rotz") {
      t = nvmath::rotation_mat4_z(nv_to_rad * float(singleton["value"]));
    } else if (singleton["type"] == "rotate") {
      vec3 xyz = Json2Vec3(singleton["value"]);
      t *= nvmath::rotation_mat4_z(nv_to_rad * xyz.z);
      t *= nvmath::rotation_mat4_y(nv_to_rad * xyz.y);
      t *= nvmath::rotation_mat4_x(nv_to_rad * xyz.x);
    } else {
      LOG_ERROR("{}: unrecognized toworld singleton type [{}]", "Loader",
                singleton["type"]);
      exit(1);
    }
    transform = t * transform;
  }
}

void Loader::addInstance(const nlohmann::json& instanceJson) {
  JsonCheckKeys(instanceJson, {"mesh"});
  string meshName = instanceJson["mesh"], materialName;
  if (instanceJson.count("material"))
    materialName = instanceJson["material"];
  else
    materialName = defaultSceneOptions["materials"]["name"];
  nvmath::mat4f transform = nvmath::mat4f_id;
  if (instanceJson.contains("toworld"))
    parseToWorld(instanceJson["toworld"], transform);
  m_pScene->addInstance(transform, meshName, materialName);
}

void Loader::addShot(const nlohmann::json& shotJson) {
  JsonCheckKeys(shotJson, {"type"});
  vec3 eye, lookat, up;
  mat4 ext = nvmath::mat4f_zero;
  if (shotJson["type"] == "lookat") {
    JsonCheckKeys(shotJson, {"eye", "lookat", "up"});
    eye = Json2Vec3(shotJson["eye"]);
    lookat = Json2Vec3(shotJson["lookat"]);
    up = Json2Vec3(shotJson["up"]);
  } else if (shotJson["type"] == "opencv") {
    JsonCheckKeys(shotJson, {"matrix", "up"});
    ext = Json2Mat4(shotJson["matrix"]);
    // ext.a00 = -ext.a00, ext.a01 = -ext.a01, ext.a02 = -ext.a02, ext.a03 =
    // -ext.a03; ext.a20 = -ext.a20, ext.a21 = -ext.a21, ext.a22 = -ext.a22,
    // ext.a23 = -ext.a23;
  } else {
    LOG_ERROR("{}: unrecognized shot type [{}]", "Loader", shotJson["type"]);
    exit(1);
  }

  CameraShot shot;
  shot.ext = ext;
  shot.eye = eye;
  shot.up = up;
  shot.lookat = lookat;
  shot.state = m_pScene->getPipelineState();  // default state

  if (shotJson.contains("state")) {
    const auto& stateJson = shotJson["state"];
    State& pipelineState = shot.state;
    parseState(stateJson, pipelineState);
  }

  if (shotJson.contains("env_toworld"))
    parseToWorld(shotJson["env_toworld"], shot.envTransform, true);

  m_pScene->addShot(shot);
}

void Loader::addEnvMap(const nlohmann::json& envmapJson) {
  JsonCheckKeys(envmapJson, {"path"});
  auto texturePath = nvh::findFile(envmapJson["path"], {m_sceneFileDir}, true);
  if (texturePath.empty()) {
    LOG_ERROR("{}: failed to load envmap from file [{%s}]", "Loader",
              envmapJson["path"]);
    exit(1);
  }
  m_pScene->addEnvMap(texturePath);
}
