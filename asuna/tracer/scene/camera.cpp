#include "camera.h"
#include <nvh/cameramanipulator.hpp>

Camera::Camera(CameraType camType, vec4 fxfycxcy, float fov)
    : m_type(camType)
{
  setFov(fov);
  m_fxfycxcy = fxfycxcy;
}

void Camera::setFov(float fov)
{
  CameraManip.setFov(fov);
}

void Camera::setToWorld(const vec3& lookat, const vec3& eye, const vec3& up)
{
  CameraManip.setLookat(eye, lookat, up);
}

void Camera::setToWorld(CameraShot shot)
{
  if(shot.ext.a33 == 1.f)
    m_ext = shot.ext;
  else
    setToWorld(shot.lookat, shot.eye, shot.up);
}

const mat4& Camera::getView()
{
  if(m_type != CameraTypeOpencv)
    return CameraManip.getMatrix();
  else
    return m_ext;
}

mat4 Camera::getProj(float aspectRatio, float nearz, float farz)
{
  return nvmath::perspectiveVK(CameraManip.getFov(), aspectRatio, nearz, farz);
}

void Camera::adaptGuiSize(int w, int h)
{
  CameraManip.setWindowSize(w, h);
}
