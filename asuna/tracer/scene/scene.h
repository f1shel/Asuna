#pragma once

#include "../../hostdevice/scene.h"
#include "../context/context.h"
#include "instance.h"
#include "mesh.h"
#include "sensor.h"

#include "../../third_party/json/json.hpp"

#include <map>
#include <string>
#include <vector>

class Scene
{
  public:
	void init(ContextAware *pContext);
	void create(std::string sceneFilePath);
	void deinit();

  private:
	ContextAware *m_pContext = nullptr;
	std::string   m_sceneFileDir;

	// Sensor
  private:
	Sensor *m_pSensor = nullptr;
	void    addSensor(const nlohmann::json &sensorJson);

  public:
	VkExtent2D getSensorSize();

	// Mesh
  private:
	// meshName -> { pMesh, meshId }
	std::map<std::string, std::pair<Mesh *, uint32_t>> m_meshLUT{};
	std::map<uint32_t, MeshAlloc *>                    m_meshAllocLUT{};
	void                                               addMesh(const nlohmann::json &meshJson);
	void                                               allocMesh(ContextAware *pContext, uint32_t meshId, const std::string &meshName, Mesh *pMesh, const VkCommandBuffer &cmdBuf);

  public:
	uint32_t   getMeshesNum();
	uint32_t   getMeshId(const std::string &meshName);
	MeshAlloc *getMeshAlloc(uint32_t meshId);

	// SceneDesc
  private:
	SceneDescAlloc *m_pSceneDescAlloc = nullptr;
	void            allocSceneDesc(ContextAware *pContext, const VkCommandBuffer &cmdBuf);

  public:
	nvvk::Buffer getSceneDescBuffer();

	// Instance
  private:
	std::vector<Instance *> m_instances{};
	void                    addInstance(const nlohmann::json &instanceJson);

  public:
	uint32_t                       getInstancesNum();
	const std::vector<Instance *> &getInstances();

  private:
	void parseSceneFile(std::string sceneFilePath);
	void allocScene();
	void freeRawData();
	void freeAllocData();
};