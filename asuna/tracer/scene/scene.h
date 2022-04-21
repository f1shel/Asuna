#pragma once

#include "../../hostdevice/scene.h"
#include "../context/context.h"
#include "instance.h"
#include "integrator.h"
#include "mesh.h"
#include "texture.h"
#include "json/json.hpp"

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

	// Integrator
  private:
	Integrator *m_pIntegrator = nullptr;
	void        addIntegrator(const nlohmann::json &integratorJson);

  public:
	VkExtent2D getSensorSize();

	// Camera
  private:
	CameraInterface *m_pCamera = nullptr;
	void             addCamera(const nlohmann::json &cameraJson);
	void             addCameraPerspective(const nlohmann::json &cameraJson);
	void             addCameraPinhole(const nlohmann::json &cameraJson);

  public:
	CameraInterface *getCamera();
	CameraType       getCameraType();

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

	// Shot
  private:
	std::vector<int *> m_shots{};

  private:
	void parseSceneFile(std::string sceneFilePath);
	void computeSceneDimensions();
	void fitCamera();
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