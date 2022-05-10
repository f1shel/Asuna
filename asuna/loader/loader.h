#pragma once

#include <string>
#include <vector>
#include "../tracer/scene/scene.h"
#include "json/json.hpp"

class Loader
{
  public:
    Loader(Scene *pScene);
    void loadSceneFromJson(std::string jsonFilePath, const std::vector<std::string> &root);

  private:
    void parse(const nlohmann::json& sceneJson);
    void submit();

  private:
    void addIntegrator(const nlohmann::json &integratorJson);
    void addCamera(const nlohmann::json &cameraJson);
    void addEmitter(const nlohmann::json &emitterJson);
    void addTexture(const nlohmann::json &textureJson);
    void addMaterial(const nlohmann::json &materialJson);
    void addMesh(const nlohmann::json &meshJson);
    void addInstance(const nlohmann::json &instanceJson);
    void addShot(const nlohmann::json &shotJson);

  private:
    Scene *m_pScene = nullptr;
    std::string m_sceneFileDir = "";
};