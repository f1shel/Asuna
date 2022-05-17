#pragma once

#include "../../hostdevice/camera.h"
#include <nvh/cameramanipulator.hpp>
#include <vulkan/vulkan_core.h>

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
  Camera(CameraType camType, VkExtent2D filmSize)
  {
    m_type = camType;
    m_size = filmSize;
    adaptFilm();
  }
  CameraType        getType() { return m_type; }
  mat4              getView() { return CameraManip.getMatrix(); }
  VkExtent2D        getFilmSize() { return m_size; }
  virtual GpuCamera toGpuStruct() = 0;
  void              setToWorld(const vec3& lookat, const vec3& eye, const vec3& up = {0.0f, 1.0f, 0.0f});
  void              setToWorld(CameraShot shot);
  void              adaptFilm();  // adapt gui to film size

private:
  CameraType m_type{CameraTypeUndefined};
  mat4       m_view{0.0f};  // world to camera space transformation
  VkExtent2D m_size{0, 0};
};

class CameraOpencv : public Camera
{
public:
  CameraOpencv(VkExtent2D filmSize, vec4 fxfycxcy)
      : Camera(CameraTypeOpencv, filmSize)
  {
    m_fxfycxcy = fxfycxcy;
  }
  virtual GpuCamera toGpuStruct();

private:
  vec4 m_fxfycxcy{0.0f};  // fx fy cx cy
};

class CameraPerspective : public Camera
{
public:
  CameraPerspective(VkExtent2D filmSize, float fov, float focalDist = 0.1f, float aperture = 0.0f)
      : Camera(CameraTypePerspective, filmSize)
      , m_focalDistance(focalDist)
      , m_aperture(aperture)
  {
    CameraManip.setFov(fov);
  }
  virtual GpuCamera toGpuStruct();
  float             getFov() { return CameraManip.getFov(); }

private:
  float m_focalDistance{0.1f};
  float m_aperture{0.0f};
};