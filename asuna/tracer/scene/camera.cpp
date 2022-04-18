#include "camera.h"
#include "utils.h"

void CameraGraphicsPerspective::init(float fov, nvmath::vec3f lookat, nvmath::vec3f eye,
                                     nvmath::vec3f up)
{
	CameraManip.setFov(fov);
	CameraManip.setLookat(eye, lookat, up);
}

void CameraGraphicsPerspective::init(float fov, nvmath::mat4f matrix)
{
	CameraManip.setFov(fov);
	CameraManip.setMatrix(matrix);
}

nvmath::mat4f CameraGraphicsPerspective::getView()
{
	return CameraManip.getMatrix();
}

nvmath::mat4f CameraGraphicsPerspective::getProj(float aspectRatio, float nearz, float farz)
{
	return nvmath::perspectiveVK(CameraManip.getFov(), aspectRatio, nearz, farz);
}

nvmath::vec4f CameraGraphicsPerspective::getIntrinsic()
{
	return nvmath::vec4f(0.0f);
}

void CameraVisionPinhole::init(nvmath::mat3f intrinsic, nvmath::mat4f extrinsic)
{
	m_int = getFxFyCxCy(intrinsic);
	CameraManip.setMatrix(extrinsic);
}

void CameraVisionPinhole::init(nvmath::mat3f intrinsic, nvmath::vec3f lookat, nvmath::vec3f eye,
                               nvmath::vec3f up)
{
	m_int = getFxFyCxCy(intrinsic);
	CameraManip.setLookat(eye, lookat, up);
}

void CameraVisionPinhole::init(float fx, float fy, float cx, float cy, nvmath::vec3f lookat,
                               nvmath::vec3f eye, nvmath::vec3f up)
{
	m_int = nvmath::vec4f(fx, fy, cx, cy);
	CameraManip.setLookat(eye, lookat, up);
}

void CameraVisionPinhole::init(float fx, float fy, float cx, float cy, nvmath::mat4f extrinsic)
{
	m_int = nvmath::vec4f(fx, fy, cx, cy);
	CameraManip.setMatrix(extrinsic);
}

nvmath::mat4f CameraVisionPinhole::getView()
{
	return CameraManip.getMatrix();
}

nvmath::mat4f CameraVisionPinhole::getProj(float aspectRatio, float nearz, float farz)
{
	return nvmath::perspectiveVK(CameraManip.getFov(), aspectRatio, nearz, farz);
}

nvmath::vec4f CameraVisionPinhole::getIntrinsic()
{
	return m_int;
}
