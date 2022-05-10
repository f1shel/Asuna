#include "loader.h"
#include "utils.h"

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
		"max_path_depth": 2
	},
	"camera": {
		"fov": 45.0
	},
	"textures": {
		"gamma": 1.0
	},
	"materials": {
	}
}
)");

Loader::Loader(Scene *pScene) : m_pScene(pScene)
{}

void Loader::loadSceneFromJson(std::string sceneFilePath, const std::vector<std::string> &root)
{
    nvh::Stopwatch sw_;
    bool           isRelativePath = path(sceneFilePath).is_relative();
    if (isRelativePath)
        sceneFilePath = nvh::findFile(sceneFilePath, root, true);
    if (sceneFilePath.empty())
        exit(1);
    m_sceneFileDir = path(sceneFilePath).parent_path().string();

    ifstream sceneFileStream(sceneFilePath);
    json     sceneFileJson;
    sceneFileStream >> sceneFileJson;

    m_pScene->reset();
    parse(sceneFileJson);
    submit();
}

void Loader::parse(const nlohmann::json &sceneFileJson)
{
    JsonCheckKeys(sceneFileJson, {"integrator", "camera", "emitters", "meshes", "instances"});

    auto &integratorJson = sceneFileJson["integrator"];
    auto &cameraJson     = sceneFileJson["camera"];
    auto &emittersJson   = sceneFileJson["emitters"];
    auto &meshesJson     = sceneFileJson["meshes"];
    auto &instancesJson  = sceneFileJson["instances"];

    // parse scene file to generate raw data
    // integrator
    addIntegrator(integratorJson);
    // camera
    addCamera(cameraJson);
    // textures
    if (sceneFileJson.contains("textures"))
    {
        auto &texturesJson = sceneFileJson["textures"];
        for (auto &textureJson : texturesJson)
        {
            addTexture(textureJson);
        }
    }

    // materials
    if (sceneFileJson.contains("materials"))
    {
        auto &materialsJson = sceneFileJson["materials"];
        for (auto &materialJson : materialsJson)
        {
            addMaterial(materialJson);
        }
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
    if (sceneFileJson.contains("shots"))
    {
        auto &shotsJson = sceneFileJson["shots"];
        for (auto &shotJson : shotsJson)
        {
            addShot(shotJson);
        }
    }
}

void Loader::submit()
{
    m_pScene->submit();
}

void Loader::addIntegrator(const nlohmann::json &integratorJson)
{
    JsonCheckKeys(integratorJson, {"width", "height"});
    VkExtent2D size = {integratorJson["width"], integratorJson["height"]};
    if (!(size.width >= 1 && size.height >= 1))
    {
        // TODO
        exit(1);
    }
    int spp      = defaultSceneOptions["integrator"]["spp"];
    int maxDepth = defaultSceneOptions["integrator"]["max_path_depth"];
    if (integratorJson.contains("spp"))
        spp = integratorJson["spp"];
    if (integratorJson.contains("max_path_depth"))
        maxDepth = integratorJson["max_path_depth"];

    m_pScene->addIntegrator(size, spp, maxDepth);
}

void Loader::addCamera(const nlohmann::json &cameraJson)
{
    JsonCheckKeys(cameraJson, {"type"});
    nvmath::vec4f fxfycxcy = 0.0f;
    float         fov      = defaultSceneOptions["camera"]["fov"];
    CameraType    camType  = eCameraTypeUndefined;
    if (cameraJson["type"] == "perspective")
    {
        if (cameraJson.contains("fov"))
            fov = cameraJson["fov"];
        camType = eCameraTypePerspective;
    }
    else if (cameraJson["type"] == "pinhole")
    {
        JsonCheckKeys(cameraJson, {"fx", "fy", "cx", "cy"});
        fxfycxcy = {cameraJson["fx"], cameraJson["fy"], cameraJson["cx"], cameraJson["cy"]};
        camType  = eCameraTypePinhole;
    }
    else
    {
        // TODO
        exit(1);
    }

    m_pScene->addCamera(camType, fxfycxcy, fov);
}

void Loader::addEmitter(const nlohmann::json &emitterJson)
{
    JsonCheckKeys(emitterJson, {"type"});
    GPUEmitter emitter;
    if (emitterJson["type"] == "distant")
    {
        JsonCheckKeys(emitterJson, {"emittance", "direction"});
        emitter.direction = Json2Vec3(emitterJson["direction"]);
        emitter.emittance = Json2Vec3(emitterJson["emittance"]);
    }
    else
    {
        // TODO
        exit(1);
    }

    m_pScene->addEmitter(emitter);
}

void Loader::addTexture(const nlohmann::json &textureJson)
{
    JsonCheckKeys(textureJson, {"name", "path"});
    std::string textureName = textureJson["name"];
    auto        texturePath = nvh::findFile(textureJson["path"], {m_sceneFileDir}, true);
    if (texturePath.empty())
    {
        LOGE("[x] %-20s: failed to load texture from %s\n", "Scene Error",
             std::string(textureJson["path"]).c_str());
        exit(1);
    }
    float gamma = defaultSceneOptions["textures"]["gamma"];
    if (textureJson.contains("gamma"))
        gamma = textureJson["gamma"];

    m_pScene->addTexture(textureName, texturePath, gamma);
}

void Loader::addMaterial(const nlohmann::json &materialJson)
{
    JsonCheckKeys(materialJson, {"type", "name"});
    std::string materialName = materialJson["name"];
    GPUMaterial material;
    if (materialJson["type"] == "brdf_hongzhi")
    {
        JsonCheckKeys(materialJson, {"diffuse_texture", "specular_texture", "normal_texture",
                                     "tangent_texture", "alpha_texture"});
        material.diffuseTextureId  = m_pScene->getTextureId(materialJson["diffuse_texture"]);
        material.specularTextureId = m_pScene->getTextureId(materialJson["specular_texture"]);
        material.normalTextureId   = m_pScene->getTextureId(materialJson["normal_texture"]);
        material.tangentTextureId  = m_pScene->getTextureId(materialJson["tangent_texture"]);
        material.alphaTextureId    = m_pScene->getTextureId(materialJson["alpha_texture"]);
    }
    else if (materialJson["type"] == "brdf_lambertian")
    {
        JsonCheckKeys(materialJson, {"diffuse_reflectance"});
        material.diffuse = Json2Vec3(materialJson["diffuse_reflectance"]);
    }
    else
    {
        // TODO
        exit(1);
    }

    m_pScene->addMaterial(materialName, material);
}

void Loader::addMesh(const nlohmann::json &meshJson)
{
    JsonCheckKeys(meshJson, {"name", "path"});
    std::string meshName = meshJson["name"];
    auto        meshPath = nvh::findFile(meshJson["path"], {m_sceneFileDir}, true);
    if (meshPath.empty())
    {
        // TODO: LOGE
        exit(1);
    }
    bool recomputeNormal = false;
    if (meshJson.contains("recompute_normal"))
        recomputeNormal = meshJson["recompute_normal"];

    m_pScene->addMesh(meshName, meshPath, recomputeNormal);
}

void Loader::addInstance(const nlohmann::json &instanceJson)
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

    m_pScene->addInstance(transform, meshName, materialName);
}

void Loader::addShot(const nlohmann::json &shotJson)
{
    JsonCheckKeys(shotJson, {"eye", "lookat", "up"});

    m_pScene->addShot(Json2Vec3(shotJson["eye"]), Json2Vec3(shotJson["lookat"]),
                      Json2Vec3(shotJson["up"]));
}
