#pragma once

#include <nvmath/nvmath.h>
#include <iostream>
#include <ext/json.hpp>
#include <nvh/nvprint.hpp>
#include <vector>

inline nvmath::vec2f Json2Vec2(const nlohmann::json& json) {
  if (json.size() < 2) {
    LOGE("[x] %-20s: failed to extract vector2f in following json segment:\n",
         "Scene Error");
    std::cout << json.dump(4) << std::endl << std::endl;
    exit(1);
  }
  return nvmath::vec2f(json[0], json[1]);
}

inline nvmath::vec3f Json2Vec3(const nlohmann::json& json) {
  if (json.size() < 3) {
    LOGE("[x] %-20s: failed to extract vector3f in following json segment:\n",
         "Scene Error");
    std::cout << json.dump(4) << std::endl << std::endl;
    exit(1);
  }
  return nvmath::vec3f(json[0], json[1], json[2]);
}

inline nvmath::mat4f Json2Mat4(const nlohmann::json& json) {
  if (json.size() < 16) {
    LOGE("[x] %-20s: failed to extract 4x4 matrix in following json segment:\n",
         "Scene Error");
    std::cout << json.dump(4) << std::endl << std::endl;
    exit(1);
  }
  return nvmath::mat4f(json[0], json[4], json[8], json[12], json[1], json[5],
                       json[9], json[13], json[2], json[6], json[10], json[14],
                       json[3], json[7], json[11], json[15]);
}

inline nvmath::mat3f Json2Mat3(const nlohmann::json& json) {
  if (json.size() < 9) {
    LOGE("[x] %-20s: failed to extract 3x3 matrix in following json segment:\n",
         "Scene Error");
    std::cout << json.dump(4) << std::endl << std::endl;
    exit(1);
  }
  return nvmath::mat3f(json[0], json[3], json[6], json[1], json[4], json[7],
                       json[2], json[5], json[8]);
}

inline nvmath::vec4f getFxFyCxCy(const nvmath::mat3f& intrinsic) {
  return nvmath::vec4f(intrinsic.a00, intrinsic.a11, intrinsic.a02,
                       intrinsic.a12);
}

inline void JsonCheckKeys(const nlohmann::json& json,
                          const std::vector<std::string>&& keys) {
  for (auto& key : keys) {
    if (!json.count(key)) {
      LOGE("[x] %-20s: missing key [%s] in following json segment:\n",
           "Scene Error", key.c_str());
      std::cout << json.dump(4) << std::endl << std::endl;
      exit(1);
    }
  }
}