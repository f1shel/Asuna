#pragma once

#include "../context/context.h"
#include "alloc.h"
#include <nvvk/commands_vk.hpp>
#include <nvvk/images_vk.hpp>
#include <nvvk/resourceallocator_vk.hpp>

class Texture
{
  public:
	Texture(const std::string &texturePath, float gamma = 1.0);
	~Texture();
	VkExtent2D getSize()
	{
		return m_shape;
	}
	VkFormat getFormat()
	{
		return m_format;
	}
	void* getData()
	{
		return m_data;
	}

  private:
	void      *m_data{nullptr};
	VkExtent2D m_shape{0};
	VkFormat   m_format{VK_FORMAT_UNDEFINED};
};

class TextureAlloc : public GPUAlloc
{
  public:
	TextureAlloc(ContextAware *pContext, Texture *pTexture, const VkCommandBuffer &cmdBuf);
	void deinit(ContextAware *pContext);
	nvvk::Texture getTexture()
	{
		return m_texture;
	}
  private:
	nvvk::Texture m_texture;
};