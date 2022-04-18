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
	if (instanceJson.count("transform"))
	{
		// TODO
	}
	Instance *pInst = new Instance;
	pInst->init(transform, getMeshId(meshName));
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

	auto &sensorJson    = sceneFileJson["sensor"];
	auto &meshesJson    = sceneFileJson["meshes"];
	auto &instancesJson = sceneFileJson["instances"];

	// parse scene file to generate raw data
	// sensor
	addSensor(sensorJson);
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
		LOGE("glTF: Scene bounding box invalid, Setting to: [-1,-1,-1], [1,1,1]");
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
	auto m_size = m_pSensor->getSize();
	CameraManip.fit(m_dimensions.min, m_dimensions.max, true, false,
	                m_size.width / static_cast<float>(m_size.height));
}

void Scene::allocScene()
{
	// allocate resources on gpu
	nvvk::CommandPool cmdBufGet(m_pContext->getDevice(), m_pContext->getQueueFamily());
	VkCommandBuffer   cmdBuf = cmdBufGet.createCommandBuffer();

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
	if (m_pSensor->getCamera() == nullptr || m_pSensor->needAutofit())
	{
		computeSceneDimensions();
		fitCamera();
	}
}

void Scene::freeRawData()
{
	// free sensor raw data
	m_pSensor->deinit();
	delete m_pSensor;
	m_pSensor = nullptr;

	// free meshes raw data
	for (auto &record : m_meshLUT)
	{
		const auto &valuePair = record.second;
		auto        pMesh     = valuePair.first;
		pMesh->deinit();
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

void Scene::addSensor(const nlohmann::json &sensorJson)
{
	m_pSensor       = new Sensor;
	VkExtent2D size = {sensorJson["width"], sensorJson["height"]};
	if (!(size.width >= 1 && size.height >= 1))
	{
		// TODO
		exit(1);
	}
	m_pSensor->init(size, sensorJson["camera"]);
}

CameraInterface *Scene::getCamera()
{
	return m_pSensor->getCamera();
}

CameraType Scene::getCameraType()
{
	return m_pSensor->getCameraType();
}

VkExtent2D Scene::getSensorSize()
{
	return m_pSensor->getSize();
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
	Mesh *pMesh = new Mesh;
	pMesh->init(meshPath);
	m_meshLUT[meshName] = std::make_pair(pMesh, m_meshLUT.size());
}

void Scene::allocMesh(ContextAware *pContext, uint32_t meshId, const string &meshName,
                      Mesh *pMesh, const VkCommandBuffer &cmdBuf)
{
	auto &m_debug  = pContext->m_debug;
	auto  m_device = pContext->getDevice();

	MeshAlloc *pMeshAlloc = new MeshAlloc;
	pMeshAlloc->init(pContext, pMesh, cmdBuf);
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
	m_pSceneDescAlloc = new SceneDescAlloc;
	m_pSceneDescAlloc->init(pContext, m_meshAllocLUT, cmdBuf);
}

nvvk::Buffer Scene::getSceneDescBuffer()
{
	return m_pSceneDescAlloc->m_bSceneDesc;
}
