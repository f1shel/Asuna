#include "scene.h"

#include <nvmath/nvmath.h>
#include <nvh/fileoperations.hpp>
#include <nvh/timesampler.hpp>
#include <nvvk/buffers_vk.hpp>
#include <nvvk/commands_vk.hpp>

#include <filesystem>
#include <fstream>

using nlohmann::json;
using std::ifstream;
using std::string;
using std::filesystem::path;

static json defaultSceneOptions = json::parse(R"(
{
	"integrator": {
		"spp": 1,
		"max_recursion": 2
	},
	"camera": {
		"fov": 45.0
	},
	"textures": {
		"gamma": 1.0,
		"name": "add_by_default_when_no_texture_exists"
	},
	"materials": {
		"type": "brdf_lambertian",
		"name": "add_by_default_and_apply_to_every_instance_without_material"
	}
}
)");

void Scene::init(ContextAware *pContext)
{
    m_pContext = pContext;
}

void Scene::create(std::string sceneFilePath)
{
    nvh::Stopwatch sw_;
    bool           isRelativePath = path(sceneFilePath).is_relative();
    if (isRelativePath)
        sceneFilePath = nvh::findFile(sceneFilePath, m_pContext->m_root, true);
    if (sceneFilePath.empty())
        exit(1);
    m_sceneFileDir = path(sceneFilePath).parent_path().string();
    parseSceneFile(sceneFilePath);
    allocScene();
    LOGI("[ ] %-20s: %6.2fms Scene file parsing and resources creation\n", "Scene",
         sw_.elapsed());
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

void Scene::addInstance(const nlohmann::json &instanceJson)
{
    JsonCheckKeys(instanceJson, {"mesh"});
    string meshName = instanceJson["mesh"], materialName;
    if (instanceJson.count("material"))
        materialName = instanceJson["material"];
    else
        materialName = defaultSceneOptions["materials"]["name"];
    nvmath::mat4f transform = nvmath::mat4f_id;
    if (instanceJson.contains("toworld"))
    {
        for (const auto &singleton : instanceJson["toworld"])
        {
            nvmath::mat4f t = nvmath::mat4f_id;
            JsonCheckKeys(singleton, {"type", "value"});
            if (singleton["type"] == "matrix")
            {
                t = Json2Mat4(singleton["value"]);
            }
            else if (singleton["type"] == "translate")
            {
                t.set_translation(Json2Vec3(singleton["value"]));
            }
            else if (singleton["type"] == "scale")
            {
                t.set_scale(Json2Vec3(singleton["scale"]));
            }
            else if (singleton["type"] == "rotx")
            {
                t = nvmath::rotation_mat4_x(float(singleton["value"]));
            }
            else if (singleton["type"] == "roty")
            {
                t = nvmath::rotation_mat4_y(float(singleton["value"]));
            }
            else if (singleton["type"] == "rotz")
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
    Instance *pInst = new Instance(transform, getMeshId(meshName), getMaterialId(materialName));
    m_instances.emplace_back(pInst);
}

uint32_t Scene::getInstancesNum()
{
    return m_instances.size();
}

const std::vector<Instance *> &Scene::getInstances()
{
    return m_instances;
}

void Scene::addShot(const nlohmann::json &shotJson)
{
    JsonCheckKeys(shotJson, {"eye", "lookat", "up"});
    m_shots.emplace_back(CameraShot{Json2Vec3(shotJson["lookat"]), Json2Vec3(shotJson["eye"]),
                                    Json2Vec3(shotJson["up"])});
}

void Scene::parseSceneFile(std::string sceneFilePath)
{
    ifstream sceneFileStream(sceneFilePath);
    json     sceneFileJson;
    sceneFileStream >> sceneFileJson;

    JsonCheckKeys(sceneFileJson,
                  {"integrator", "camera", "emitters", "meshes", "instances", "shots"});

    auto &integratorJson = sceneFileJson["integrator"];
    auto &cameraJson     = sceneFileJson["camera"];
    auto &emittersJson   = sceneFileJson["emitters"];
    auto &texturesJson   = sceneFileJson["textures"];
    auto &materialsJson  = sceneFileJson["materials"];
    auto &meshesJson     = sceneFileJson["meshes"];
    auto &instancesJson  = sceneFileJson["instances"];
    auto &shotsJson      = sceneFileJson["shots"];

    // parse scene file to generate raw data
    // integrator
    addIntegrator(integratorJson);
    // camera
    addCamera(cameraJson);
    // textures
    addDummyTexture();        // if no texture or material exists, pipeline will complain
    for (auto &textureJson : texturesJson)
    {
        addTexture(textureJson);
    }
    // materials
    addDummyMaterial();        // if no texture or material exists, pipeline will complain
    for (auto &materialJson : materialsJson)
    {
        addMaterial(materialJson);
    }
    for (auto &emitterJson : emittersJson)
    {
        addEmitter(emitterJson);
    }
    // meshes
    for (auto &meshJson : meshesJson)
    {
        addMesh(meshJson);
    }
    // instances
    for (auto &instanceJson : instancesJson)
    {
        addInstance(instanceJson);
    }
    // shots
    for (auto &shotJson : shotsJson)
    {
        addShot(shotJson);
    }
}

void Scene::computeSceneDimensions()
{
    Bbox scnBbox;

    for (const auto pInstance : m_instances)
    {
        auto pMeshAlloc = m_meshAllocLUT[pInstance->m_meshIndex];
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

void Scene::addDummyTexture()
{
    const string &textureName = defaultSceneOptions["textures"]["name"];
    Texture      *pTexture    = new Texture;
    m_textureLUT[textureName] = std::make_pair(pTexture, m_textureLUT.size());
}

void Scene::addDummyMaterial()
{
    const string      &materialName = defaultSceneOptions["materials"]["name"];
    MaterialInterface *pMaterial    = new MaterialBrdfLambertian;
    m_materialLUT[materialName]     = std::make_pair(pMaterial, m_materialLUT.size());
}

void Scene::allocScene()
{
    // allocate resources on gpu
    nvvk::CommandPool cmdBufGet(m_pContext->getDevice(), m_pContext->getQueueFamily());
    VkCommandBuffer   cmdBuf = cmdBufGet.createCommandBuffer();

    allocEmitters(m_pContext, cmdBuf);

    for (auto &record : m_textureLUT)
    {
        const auto &textureName = record.first;
        const auto &valuePair   = record.second;
        auto        pTexture    = valuePair.first;
        auto        textureId   = valuePair.second;
        allocTexture(m_pContext, textureId, textureName, pTexture, cmdBuf);
    }

    for (auto &record : m_materialLUT)
    {
        const auto &materialName = record.first;
        const auto &valuePair    = record.second;
        auto        pMaterial    = valuePair.first;
        auto        materialId   = valuePair.second;
        allocMaterial(m_pContext, materialId, materialName, pMaterial, cmdBuf);
    }

    for (auto &record : m_meshLUT)
    {
        const auto &meshName  = record.first;
        const auto &valuePair = record.second;
        auto        pMesh     = valuePair.first;
        auto        meshId    = valuePair.second;
        allocMesh(m_pContext, meshId, meshName, pMesh, cmdBuf);
    }

    // Keeping the mesh description at host and device
    allocSceneDesc(m_pContext, cmdBuf);

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
}

void Scene::freeRawData()
{
    // free sensor raw data
    delete m_pIntegrator;
    m_pIntegrator = nullptr;

    delete m_pCamera;
    m_pCamera = nullptr;

    // free meshes raw data
    for (auto &record : m_meshLUT)
    {
        const auto &valuePair = record.second;
        auto        pMesh     = valuePair.first;
        delete pMesh;
    }

    // free instance raw data
    for (auto &pInst : m_instances)
    {
        delete pInst;
        pInst = nullptr;
    }
    m_instances.clear();
    m_shots.clear();
    m_emitters.clear();
}

void Scene::freeAllocData()
{
    m_pEmitterAlloc->deinit(m_pContext);
    delete m_pEmitterAlloc;
    m_pEmitterAlloc = nullptr;

    // free textures alloc data
    for (auto &record : m_textureAllocLUT)
    {
        auto pTextureAlloc = record.second;
        pTextureAlloc->deinit(m_pContext);
    }

    // free meshes alloc data
    for (auto &record : m_meshAllocLUT)
    {
        auto pMeshAlloc = record.second;
        pMeshAlloc->deinit(m_pContext);
    }

    // free scene desc alloc data
    m_pSceneDescAlloc->deinit(m_pContext);
    delete m_pSceneDescAlloc;
    m_pSceneDescAlloc = nullptr;
}

void Scene::addIntegrator(const nlohmann::json &integratorJson)
{
    JsonCheckKeys(integratorJson, {"width", "height"});
    VkExtent2D size = {integratorJson["width"], integratorJson["height"]};
    if (!(size.width >= 1 && size.height >= 1))
    {
        // TODO
        exit(1);
    }
    int spp      = defaultSceneOptions["integrator"]["spp"];
    int maxRecur = defaultSceneOptions["integrator"]["max_recursion"];
    if (integratorJson.contains("spp"))
        spp = integratorJson["spp"];
    if (integratorJson.contains("max_recursion"))
        maxRecur = integratorJson["max_recursion"];
    m_pIntegrator = new Integrator(size, spp, maxRecur);
}

void Scene::addCamera(const nlohmann::json &cameraJson)
{
    JsonCheckKeys(cameraJson, {"type"});
    if (cameraJson["type"] == "perspective")
        addCameraPerspective(cameraJson);
    else if (cameraJson["type"] == "pinhole")
        addCameraPinhole(cameraJson);
    else
    {
        // TODO
        exit(1);
    }
}

void Scene::addCameraPerspective(const nlohmann::json &cameraJson)
{
    float fov = defaultSceneOptions["camera"]["fov"];
    if (cameraJson.contains("fov"))
        fov = cameraJson["fov"];
    m_pCamera = new CameraGraphicsPerspective(fov);
}

void Scene::addCameraPinhole(const nlohmann::json &cameraJson)
{
    nvmath::vec4f fxfycxcy = 0.0f;
    if (cameraJson.contains("intrinsic"))
    {
        auto intrinsic = Json2Mat3(cameraJson["intrinsic"]);
        fxfycxcy       = getFxFyCxCy(intrinsic);
    }
    else if (cameraJson.contains("fx") && cameraJson.contains("fy") &&
             cameraJson.contains("cx") && cameraJson.contains("cy"))
    {
        fxfycxcy = {cameraJson.contains("fx"), cameraJson.contains("fy"),
                    cameraJson.contains("cx"), cameraJson.contains("cy")};
    }
    else
    {
        // LOGE("TODO");
        exit(1);
    }
    m_pCamera = new CameraVisionPinhole(fxfycxcy.x, fxfycxcy.y, fxfycxcy.z, fxfycxcy.w);
}

CameraInterface *Scene::getCamera()
{
    return m_pCamera;
}

CameraType Scene::getCameraType()
{
    return m_pCamera->getType();
}

void Scene::addEmitter(const nlohmann::json &emitterJson)
{
    JsonCheckKeys(emitterJson, {"type"});
    if (emitterJson["type"] == "distant")
        addEmitterDistant(emitterJson);
    else
    {
        // TODO
        exit(1);
    }
}

void Scene::addEmitterDistant(const nlohmann::json &emitterJson)
{
    JsonCheckKeys(emitterJson, {"emittance", "direction"});
    GPUEmitter emitter;
    emitter.direction = Json2Vec3(emitterJson["direction"]);
    emitter.emittance = Json2Vec3(emitterJson["emittance"]);
    m_emitters.emplace_back(emitter);
}

void Scene::allocEmitters(ContextAware *pContext, const VkCommandBuffer &cmdBuf)
{
    m_pEmitterAlloc = new EmitterAlloc(pContext, m_emitters, cmdBuf);
}

EmitterAlloc *Scene::getEmitterAlloc()
{
    return m_pEmitterAlloc;
}

int Scene::getEmittersNum()
{
    return m_emitters.size();
}

void Scene::addTexture(const nlohmann::json &textureJson)
{
    JsonCheckKeys(textureJson, {"name", "path"});
    std::string textureName = textureJson["name"];
    if (m_meshLUT.count(textureName))
    {
        LOGE("[x] %-20s: texture %s already exists\n", "Scene Error", textureName.c_str());
        exit(1);
    }
    auto texturePath = nvh::findFile(textureJson["path"], {m_sceneFileDir}, true);
    if (texturePath.empty())
    {
        LOGE("[x] %-20s: failed to load texture from %s\n", "Scene Error",
             std::string(textureJson["path"]).c_str());
        exit(1);
    }
    float gamma = defaultSceneOptions["textures"]["gamma"];
    if (textureJson.contains("gamma"))
        gamma = textureJson["gamma"];

    Texture *pTexture         = new Texture(texturePath, gamma);
    m_textureLUT[textureName] = std::make_pair(pTexture, m_textureLUT.size());
}

void Scene::allocTexture(ContextAware *pContext, uint32_t textureId,
                         const std::string &textureName, Texture *pTexture,
                         const VkCommandBuffer &cmdBuf)
{
    auto &m_debug  = pContext->m_debug;
    auto  m_device = pContext->getDevice();

    TextureAlloc *pTextureAlloc  = new TextureAlloc(pContext, pTexture, cmdBuf);
    m_textureAllocLUT[textureId] = pTextureAlloc;
}

uint32_t Scene::getTexturesNum()
{
    return m_textureLUT.size();
}

uint32_t Scene::getTextureId(const std::string &textureName)
{
    return m_textureLUT[textureName].second;
}

TextureAlloc *Scene::getTextureAlloc(uint32_t textureId)
{
    return m_textureAllocLUT[textureId];
}

void Scene::addMaterial(const nlohmann::json &materialJson)
{
    JsonCheckKeys(materialJson, {"type", "name"});
    std::string materialName = materialJson["name"];
    if (m_materialLUT.count(materialName))
    {
        LOGE("[x] %-20s: material %s already exists\n", "Scene Error", materialName.c_str());
        exit(1);
    }
    if (materialJson["type"] == "brdf_hongzhi")
        addMaterialBrdfHongzhi(materialJson);
    else if (materialJson["type"] == "brdf_lambertian")
        addMaterialBrdfLambertian(materialJson);
    else
    {
        // TODO
        exit(1);
    }
}

void Scene::addMaterialBrdfHongzhi(const nlohmann::json &materialJson)
{
    std::string materialName = materialJson["name"];
    JsonCheckKeys(materialJson, {"diffuse_texture", "specular_texture", "normal_texture",
                                 "tangent_texture", "alpha_texture"});
    int                diffuseTextureId  = getTextureId(materialJson["diffuse_texture"]);
    int                specularTextureId = getTextureId(materialJson["specular_texture"]);
    int                normalTextureId   = getTextureId(materialJson["normal_texture"]);
    int                tangentTextureId  = getTextureId(materialJson["tangent_texture"]);
    int                alphaTextureId    = getTextureId(materialJson["alpha_texture"]);
    MaterialInterface *pMaterial         = new MaterialBrdfHongzhi(
                diffuseTextureId, specularTextureId, alphaTextureId, normalTextureId, tangentTextureId);
    m_materialLUT[materialName] = std::make_pair(pMaterial, m_materialLUT.size());
}

void Scene::addMaterialBrdfLambertian(const nlohmann::json &materialJson)
{
    std::string materialName = materialJson["name"];
    JsonCheckKeys(materialJson, {"diffuse_reflectance"});
    vec3               diffuse   = Json2Vec3(materialJson["diffuse_reflectance"]);
    MaterialInterface *pMaterial = new MaterialBrdfLambertian(diffuse);
    m_materialLUT[materialName]  = std::make_pair(pMaterial, m_materialLUT.size());
}

void Scene::allocMaterial(ContextAware *pContext, uint32_t materialId,
                          const std::string &materialName, MaterialInterface *pMaterial,
                          const VkCommandBuffer &cmdBuf)
{
    auto &m_debug  = pContext->m_debug;
    auto  m_device = pContext->getDevice();

    MaterialAlloc *pMaterialAlloc  = new MaterialAlloc(pContext, pMaterial, cmdBuf);
    m_materialAllocLUT[materialId] = pMaterialAlloc;

    m_debug.setObjectName(pMaterialAlloc->getBuffer(),
                          std::string(materialName + "_materialBuffer"));
}

uint32_t Scene::getMaterialsNum()
{
    return m_materialLUT.size();
}

uint32_t Scene::getMaterialId(const std::string &materialName)
{
    return m_materialLUT[materialName].second;
}

MaterialAlloc *Scene::getMaterialAlloc(uint32_t materialId)
{
    return m_materialAllocLUT[materialId];
}

VkExtent2D Scene::getSize()
{
    return m_pIntegrator->getSize();
}

uint32_t Scene::getSpp()
{
    return m_pIntegrator->getSpp();
}

uint32_t Scene::getMaxRecurDepth()
{
    return m_pIntegrator->getMaxRecurDepth();
}

void Scene::addMesh(const nlohmann::json &meshJson)
{
    JsonCheckKeys(meshJson, {"name", "path"});
    std::string meshName = meshJson["name"];
    if (m_meshLUT.count(meshName))
    {
        // TODO: LOGE
        exit(1);
    }
    auto meshPath = nvh::findFile(meshJson["path"], {m_sceneFileDir}, true);
    if (meshPath.empty())
    {
        // TODO: LOGE
        exit(1);
    }
    bool recomputeNormal = false;
    if (meshJson.contains("recompute_normal"))
        recomputeNormal = meshJson["recompute_normal"];

    Mesh *pMesh         = new Mesh(meshPath, recomputeNormal);
    m_meshLUT[meshName] = std::make_pair(pMesh, m_meshLUT.size());
}

void Scene::allocMesh(ContextAware *pContext, uint32_t meshId, const string &meshName,
                      Mesh *pMesh, const VkCommandBuffer &cmdBuf)
{
    auto &m_debug  = pContext->m_debug;
    auto  m_device = pContext->getDevice();

    MeshAlloc *pMeshAlloc  = new MeshAlloc(pContext, pMesh, cmdBuf);
    m_meshAllocLUT[meshId] = pMeshAlloc;

    m_debug.setObjectName(pMeshAlloc->getVerticesBuffer(),
                          std::string(meshName + "_vertexBuffer"));
    m_debug.setObjectName(pMeshAlloc->getIndicesBuffer(),
                          std::string(meshName + "_indexBuffer"));
}

uint32_t Scene::getMeshesNum()
{
    return m_meshLUT.size();
}

uint32_t Scene::getMeshId(const std::string &meshName)
{
    if (m_meshLUT.count(meshName))
        return m_meshLUT[meshName].second;
    else
    {
        LOGE("[x] %-20s: mesh %s does not exist\n", "Scene Error", meshName.c_str());
        exit(1);
    }
}

MeshAlloc *Scene::getMeshAlloc(uint32_t meshId)
{
    return m_meshAllocLUT[meshId];
}

void Scene::allocSceneDesc(ContextAware *pContext, const VkCommandBuffer &cmdBuf)
{
    // Keeping the obj host model and device description
    m_pSceneDescAlloc =
        new SceneDescAlloc(pContext, m_instances, m_meshAllocLUT, m_materialAllocLUT, cmdBuf);
}

VkBuffer Scene::getSceneDescBuffer()
{
    return m_pSceneDescAlloc->getBuffer();
}