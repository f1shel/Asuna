#pragma once

#include <nvh/cameramanipulator.hpp>
#include "utils.h"

enum class CameraType
{
	eCameraTypeUndefined,
	eCameraTypePerspective,
	eCameraTypePinhole
};

class CameraInterface
{
  public:
	CameraInterface(CameraType type) : m_camType(type)
	{}
	virtual nvmath::vec4f getIntrinsic() = 0;
	virtual nvmath::mat4f getView()
	{
		return CameraManip.getMatrix();
	}
	virtual nvmath::mat4f getProj(float aspectRatio, float nearz = 0.1, float farz = 100)
	{
		return nvmath::perspectiveVK(CameraManip.getFov(), aspectRatio, nearz, farz);
	}
	virtual CameraType getType()
	{
		return m_camType;
	}
	virtual void setToWorld(nvmath::vec3f lookat, nvmath::vec3f eye,
	                        nvmath::vec3f up = {0.0f, 1.0f, 0.0f})
	{
		CameraManip.setLookat(eye, lookat, up);
	}
	virtual void setToWorld(nvmath::mat4f extrinsic)
	{
		CameraManip.setMatrix(extrinsic);
	}

  protected:
	CameraType m_camType = CameraType::eCameraTypeUndefined;
};

class CameraGraphicsPerspective : public CameraInterface
{
  public:
	CameraGraphicsPerspective(float fov) : CameraInterface(CameraType::eCameraTypePerspective)
	{
		CameraManip.setFov(fov);
	}
	virtual nvmath::vec4f getIntrinsic()
	{
		return nvmath::vec4f(0.0f);
	}
};

class CameraVisionPinhole : public CameraInterface
{
  public:
	CameraVisionPinhole(nvmath::mat3f intrinsic) :
	    CameraInterface(CameraType::eCameraTypePinhole)
	{
		m_int     = getFxFyCxCy(intrinsic);
	}
	CameraVisionPinhole(float fx, float fy, float cx, float cy) :
	    CameraInterface(CameraType::eCameraTypePinhole)
	{
		m_int     = vec4(fx, fy, cx, cy);
	}
	virtual nvmath::vec4f getIntrinsic()
	{
		return m_int;
	}

  private:
	nvmath::vec4f m_int{1.0f};        // fx fy cx cy
};