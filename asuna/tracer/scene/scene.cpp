#include "scene.h"

#include <nvh/fileoperations.hpp>
#include <nvmath/nvmath.h>
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
		"gamma": 1.0
	}
}
)");

void Scene::init(ContextAware *pContext)
{
	m_pContext = pContext;
}

void Scene::create(std::string sceneFilePath)
{
	bool isRelativePath = path(sceneFilePath).is_relative();
	if (isRelativePath)
		sceneFilePath = nvh::findFile(sceneFilePath, m_pContext->m_root, true);
	if (sceneFilePath.empty())
		exit(1);
	m_sceneFileDir = path(sceneFilePath).parent_path().string();
	parseSceneFile(sceneFilePath);
	allocScene();
}

void Scene::deinit()
{
	assert((m_pContext != nullptr) &&
	       "[!] Scene Error: failed to find belonging context when deinit.");
	freeRawData();
	freeAllocData();
}

void Scene::addInstance(const nlohmann::json &instanceJson)
{
	const string &meshName  = instanceJson["mesh"];
	nvmath::mat4f transform = nvmath::mat4f_id;
	if (instanceJson.contains("transform"))
	{
		for (const auto &singleton : instanceJson["transform"])
		{
			nvmath::mat4f t = nvmath::mat4f_id;
			if (!singleton.contains("type"))
			{
				// TODO
				exit(1);
			}
			if (singleton["type"] == "translate")
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
	Instance *pInst = new Instance(transform, getMeshId(meshName));
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

void Scene::parseSceneFile(std::string sceneFilePath)
{
	ifstream sceneFileStream(sceneFilePath);
	json     sceneFileJson;
	sceneFileStream >> sceneFileJson;

	auto &integratorJson = sceneFileJson["integrator"];
	auto &cameraJson     = sceneFileJson["camera"];
	auto &texturesJson   = sceneFileJson["textures"];
	auto &meshesJson     = sceneFileJson["meshes"];
	auto &instancesJson  = sceneFileJson["instances"];

	// parse scene file to generate raw data
	// integrator
	addIntegrator(integratorJson);
	// camera
	addCamera(cameraJson);
	// textures
	for (auto &textureJson : texturesJson)
	{
		addTexture(textureJson);
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
		LOGE("[!] Scene Warning: Scene bounding box invalid, Setting to: [-1,-1,-1], [1,1,1]");
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

void Scene::allocScene()
{
	// allocate resources on gpu
	nvvk::CommandPool cmdBufGet(m_pContext->getDevice(), m_pContext->getQueueFamily());
	VkCommandBuffer   cmdBuf = cmdBufGet.createCommandBuffer();

	for (auto &record : m_textureLUT)
	{
		const auto &textureName = record.first;
		const auto &valuePair   = record.second;
		auto        pTexture    = valuePair.first;
		auto        textureId   = valuePair.second;
		allocTexture(m_pContext, textureId, textureName, pTexture, cmdBuf);
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
	}
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
}

void Scene::freeAllocData()
{
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
	if (cameraJson.contains("type"))
	{
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

void Scene::addTexture(const nlohmann::json &textureJson)
{
	std::string textureName = textureJson["name"];
	if (m_meshLUT.count(textureName))
	{
		// TODO: LOGE
		exit(1);
	}
	auto texturePath = nvh::findFile(textureJson["path"], {m_sceneFileDir}, true);
	if (texturePath.empty())
	{
		// TODO: LOGE
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

VkExtent2D Scene::getSensorSize()
{
	return m_pIntegrator->getSize();
}

void Scene::addMesh(const nlohmann::json &meshJson)
{
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

	m_debug.setObjectName(pMeshAlloc->m_bVertices.buffer,
	                      std::string(meshName + "_vertexBuffer"));
	m_debug.setObjectName(pMeshAlloc->m_bIndices.buffer, std::string(meshName + "_indexBuffer"));
}

uint32_t Scene::getMeshesNum()
{
	return m_meshLUT.size();
}

uint32_t Scene::getMeshId(const std::string &meshName)
{
	return m_meshLUT[meshName].second;
}

MeshAlloc *Scene::getMeshAlloc(uint32_t meshId)
{
	return m_meshAllocLUT[meshId];
}

void Scene::allocSceneDesc(ContextAware *pContext, const VkCommandBuffer &cmdBuf)
{
	// Keeping the obj host model and device description
	m_pSceneDescAlloc = new SceneDescAlloc(pContext, m_meshAllocLUT, cmdBuf);
}

nvvk::Buffer Scene::getSceneDescBuffer()
{
	return m_pSceneDescAlloc->m_bSceneDesc;
}
