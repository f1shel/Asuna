#pragma once

#include <string>
#include <vector>
#include <scene/scene.h>
#include <ext/json.hpp>

class Loader {
public:
  Loader() {}

  // Load size first so we can create window in online mode
  VkExtent2D loadSizeFirst(std::string jsonFilePath, const std::string& root);

  void loadSceneFromJson(std::string jsonFilePath, const std::string& root,
                         Scene* pScene);

private:
  void parse(const nlohmann::json& sceneJson);
  void submit();

private:
  void addState(const nlohmann::json& stateJson);
  void addCamera(const nlohmann::json& cameraJson);
  void addLight(const nlohmann::json& lightJson);
  void addTexture(const nlohmann::json& textureJson);
  void addMaterial(const nlohmann::json& materialJson);
  void addMesh(const nlohmann::json& meshJson);
  void addInstance(const nlohmann::json& instanceJson);
  void addShot(const nlohmann::json& shotJson);
  void addEnvMap(const nlohmann::json& envmapJson);

private:
  Scene* m_pScene = nullptr;
  string m_sceneFileDir = "";
};