#include "sensor.h"
#include "utils.h"
using nlohmann::json;

void Sensor::init(VkExtent2D size, const nlohmann::json &cameraJson)
{
	m_size = size;
	if (cameraJson.contains("type"))
	{
		if (cameraJson["type"] == "perspective")
			m_camType = eCameraTypePerspective;
		else if (cameraJson["type"] == "pinhole")
			m_camType = eCameraTypePinhole;
		else
		{
			// TODO
			exit(1);
		}
	}
	else
	{
		// TODO
		exit(1);
	}
	if (m_camType == eCameraTypePerspective)
	{
		CameraGraphicsPerspective *pCamera = new CameraGraphicsPerspective;
		float                      fov     = 45.0f;
		if (cameraJson.contains("fov"))
			fov = cameraJson["fov"];
		if (cameraJson.contains("toworld"))
		{
			pCamera->init(fov, Json2Mat4(cameraJson["toworld"]));
		}
		else if (cameraJson.contains("lookat") && cameraJson.contains("eye"))
		{
			nvmath::vec3f up = {0.0, 1.0, 0.0};
			if (cameraJson.contains("up"))
				up = Json2Vec3(cameraJson["up"]);
			pCamera->init(fov, Json2Vec3(cameraJson["lookat"]), Json2Vec3(cameraJson["eye"]),
			              up);
		}
		else
		{
			// LOGE("TODO");
			pCamera->init(fov, nvmath::mat4f_id);
			m_autofit = true;
		}
		m_pCamera = pCamera;
	}
	if (m_camType == eCameraTypePinhole)
	{
		CameraVisionPinhole *pCam     = new CameraVisionPinhole;
		nvmath::vec4f        fxfycxcy = 0.0f;
		if (cameraJson.contains("intrinsic"))
		{
			auto intrinsic = Json2Mat3(cameraJson["intrinsic"]);
			fxfycxcy       = getFxFyCxCy(intrinsic);
		}
		else if (cameraJson.contains("fx") && cameraJson.contains("fy") &&
		         cameraJson.contains("cx") && cameraJson.contains("cy"))
		{
			fxfycxcy = {cameraJson.contains("fx"), cameraJson.contains("fy"),
			            cameraJson.contains("cx"), cameraJson.contains("cy")};
		}
		else
		{
			// LOGE("TODO");
			exit(1);
		}
		if (cameraJson.contains("toworld"))
		{
			pCam->init(fxfycxcy.x, fxfycxcy.y, fxfycxcy.z, fxfycxcy.w,
			           Json2Mat4(cameraJson["toworld"]));
		}
		else if (cameraJson.contains("eye") && cameraJson.contains("lookat"))
		{
			nvmath::vec3f up = {0.0, 1.0, 0.0};
			if (cameraJson.contains("up"))
				up = Json2Vec3(cameraJson["up"]);
			pCam->init(fxfycxcy.x, fxfycxcy.y, fxfycxcy.z, fxfycxcy.w,
			           Json2Vec3(cameraJson["lookat"]), Json2Vec3(cameraJson["eye"]), up);
		}
		else
		{
			// LOGE("TODO");
			pCam->init(fxfycxcy.x, fxfycxcy.y, fxfycxcy.z, fxfycxcy.w, nvmath::mat4f_id);
			m_autofit = true;
		}
		m_pCamera = pCam;
	}
}

void Sensor::deinit()
{
	delete m_pCamera;
	m_pCamera = nullptr;
	m_size    = {0, 0};
	m_camType = eCameraTypePerspective;
}

CameraInterface *Sensor::getCamera()
{
	return m_pCamera;
}
