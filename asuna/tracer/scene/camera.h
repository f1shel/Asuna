#pragma once

#include <nvh/cameramanipulator.hpp>

enum CameraType
{
	eCameraTypePerspective,
	eCameraTypePinhole,
	eCameraTypeCount
};

class CameraInterface
{
  public:
	virtual nvmath::mat4f getView()                                                       = 0;
	virtual nvmath::mat4f getProj(float aspectRatio, float nearz = 0.1, float farz = 100) = 0;
	virtual nvmath::vec4f getIntrinsic()                                                  = 0;

  private:
	float placeholder;
};

class CameraGraphicsPerspective : public CameraInterface
{
  public:
	void                  init(float fov, nvmath::vec3f lookat, nvmath::vec3f eye,
	                           nvmath::vec3f up = {0.0f, 1.0f, 0.0f});
	void                  init(float fov, nvmath::mat4f matrix);
	virtual nvmath::mat4f getView();
	virtual nvmath::mat4f getProj(float aspectRatio, float nearz = 0.1, float farz = 100);
	virtual nvmath::vec4f getIntrinsic();
};

class CameraVisionPinhole : public CameraInterface
{
  public:
	void init(nvmath::mat3f intrinsic, nvmath::mat4f extrinsic);
	void init(nvmath::mat3f intrinsic, nvmath::vec3f lookat, nvmath::vec3f eye,
	          nvmath::vec3f up = {0.0f, 1.0f, 0.0f});
	void init(float fx, float fy, float cx, float cy, nvmath::vec3f lookat,
	          nvmath::vec3f eye, nvmath::vec3f up = {0.0f, 1.0f, 0.0f});
	void init(float fx, float fy, float cx, float cy, nvmath::mat4f extrinsic);
	virtual nvmath::mat4f getView();
	virtual nvmath::mat4f getProj(float aspectRatio, float nearz = 0.1, float farz = 100);
	virtual nvmath::vec4f getIntrinsic();

  private:
	nvmath::vec4f m_int{1.0f};        // fx fy cx cy
};