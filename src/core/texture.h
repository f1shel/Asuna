#pragma once

#include <nvvk/commands_vk.hpp>
#include <nvvk/images_vk.hpp>
#include <nvvk/resourceallocator_vk.hpp>
#include <context/context.h>
#include "alloc.h"

class Texture
{
public:
  // Add default texture (size of 1x1) when no texture exists in scene
  Texture();
  Texture(const std::string& texturePath, float gamma = 1.0);
  ~Texture();
  VkExtent2D getSize() { return m_shape; }
  VkFormat   getFormat() { return m_format; }
  void*      getData() { return m_data; }

private:
  void*      m_data{nullptr};
  VkExtent2D m_shape{0};
  VkFormat   m_format{VK_FORMAT_UNDEFINED};
};

class TextureAlloc : public GpuAlloc
{
public:
  TextureAlloc(ContextAware* pContext, Texture* pTexture, const VkCommandBuffer& cmdBuf);
  void                  deinit(ContextAware* pContext);
  VkDescriptorImageInfo getTexture() { return m_texture.descriptor; }

private:
  nvvk::Texture m_texture;
};