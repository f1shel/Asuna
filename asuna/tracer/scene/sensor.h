#pragma once

#include "camera.h"

#include "../../third_party/json/json.hpp"
#include <nvvk/appbase_vk.hpp>

class Sensor
{
  public:
	void       init(VkExtent2D size, const nlohmann::json &cameraJson);
	void       deinit();
	VkExtent2D getSize()
	{
		return m_size;
	}
	CameraInterface *getCamera();
	CameraType       getCameraType()
	{
		return m_camType;
	}
	bool needAutofit()
	{
		return m_autofit;
	}

  private:
	VkExtent2D       m_size{0, 0};
	CameraType       m_camType{eCameraTypePerspective};
	CameraInterface *m_pCamera = nullptr;
	bool             m_autofit = false;
};