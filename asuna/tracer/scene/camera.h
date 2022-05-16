#pragma once

#include "../../hostdevice/camera.h"

typedef struct
{
  vec3 lookat{0.0};
  vec3 eye{0.0};
  vec3 up{0.0};
  mat4 ext{0.0};
} CameraShot;

class Camera
{
public:
  Camera() {}
  Camera(CameraType camType, vec4 fxfycxcy, float fov);
  const vec4& getFxFyCxCy() { return m_fxfycxcy; }
  CameraType  getType() { return m_type; }
  void        setFov(float fov);
  void        setToWorld(const vec3& lookat, const vec3& eye, const vec3& up = {0.0f, 1.0f, 0.0f});
  void        setToWorld(CameraShot shot);
  const mat4& getView();
  mat4        getProj(float aspectRatio, float nearz = 0.1, float farz = 100);
  void        adaptGuiSize(int w, int h);

private:
  vec4       m_fxfycxcy{0.0f};  // fx fy cx cy
  mat4       m_ext{0.0f};
  CameraType m_type{CameraTypeUndefined};
};