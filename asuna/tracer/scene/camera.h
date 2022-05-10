#pragma once

#include <nvh/cameramanipulator.hpp>
#include "utils.h"

enum CameraType
{
    eCameraTypeUndefined   = 0,
    eCameraTypePerspective = 1,
    eCameraTypePinhole     = 2
};

typedef struct
{
    nvmath::vec3f lookat;
    nvmath::vec3f eye;
    nvmath::vec3f up;
} CameraShot;

class Camera
{
  public:
    Camera(CameraType camType, nvmath::vec4f fxfycxcy, float fov) : m_type(camType)
    {
        CameraManip.setFov(fov);
        m_int = fxfycxcy;
    }
    nvmath::vec4f getIntrinsic()
    {
        return m_int;
    }
    nvmath::mat4f getView()
    {
        return CameraManip.getMatrix();
    }
    nvmath::mat4f getProj(float aspectRatio, float nearz = 0.1, float farz = 100)
    {
        return nvmath::perspectiveVK(CameraManip.getFov(), aspectRatio, nearz, farz);
    }
    void setToWorld(nvmath::vec3f lookat, nvmath::vec3f eye,
                    nvmath::vec3f up = {0.0f, 1.0f, 0.0f})
    {
        CameraManip.setLookat(eye, lookat, up);
    }
    void setToWorld(CameraShot shot)
    {
        CameraManip.setLookat(shot.eye, shot.lookat, shot.up);
    }
    nvh::CameraManipulator::Camera getCamera()
    {
        return CameraManip.getCamera();
    }
    CameraType getType()
    {
        return m_type;
    }

  private:
    nvmath::vec4f m_int{1.0f};        // fx fy cx cy
    CameraType    m_type{eCameraTypeUndefined};
};