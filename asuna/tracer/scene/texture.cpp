#include "texture.h"

#include <nvh/nvprint.hpp>
#include <nvvk/commands_vk.hpp>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <filesystem>
using path = std::filesystem::path;

Texture::Texture()
{
	m_data   = (void *) malloc(1 * 1 * 4 * sizeof(float));
	m_format = VK_FORMAT_R32G32B32A32_SFLOAT;
	m_shape  = {(uint32_t) 1, (uint32_t) 1};
}

Texture::Texture(const std::string &texturePath, float gamma)
{
	static std::set<std::string> supportExtensions = {".hdr", ".jpg", ".png"};
	int32_t                      width{0};
	int32_t                      height{0};
	int32_t                      component{0};
	std::string                  ext = path(texturePath).extension().string();
	if (!supportExtensions.count(ext))
	{
		LOGE("[x] %-20s: textures only support extensions (.hdr .jpg .png) while %s "
		     "is passed in",
		     "Scene Error", ext.c_str());
		exit(1);
	}
	VkFormat format = VK_FORMAT_UNDEFINED;
	void    *pixels = nullptr;
	// High dynamic range image
	if (ext == ".hdr")
	{
		format = VK_FORMAT_R32G32B32A32_SFLOAT;
		pixels = (void *) stbi_loadf(texturePath.c_str(), &width, &height, &component,
		                             STBI_rgb_alpha);
	}
	// 32bit image
	else
	{
		stbi_ldr_to_hdr_gamma(gamma);
		format = VK_FORMAT_R32G32B32A32_SFLOAT;
		pixels = (void *) stbi_loadf(texturePath.c_str(), &width, &height, &component,
		                             STBI_rgb_alpha);
		stbi_ldr_to_hdr_gamma(2.0);
	}
	// Handle failure
	if (!pixels)
	{
		LOGE("[x] %-20s: failed to load %s", "Scene Error", texturePath.c_str());
		exit(1);
	}
	m_data   = pixels;
	m_format = format;
	m_shape  = {(uint32_t) width, (uint32_t) height};
}

Texture::~Texture()
{
	m_shape  = {0};
	m_format = VK_FORMAT_UNDEFINED;
	if (m_data)
	{
		if (m_format == VK_FORMAT_R32G32B32A32_SFLOAT)
		{
			float *pixels = reinterpret_cast<float *>(m_data);
			stbi_image_free(pixels);
		}
		else
		{
			stbi_uc *pixels = reinterpret_cast<stbi_uc *>(m_data);
			stbi_image_free(pixels);
		}
	}
}

TextureAlloc::TextureAlloc(ContextAware *pContext, Texture *pTexture,
                           const VkCommandBuffer &cmdBuf)
{
	auto &m_alloc = pContext->m_alloc;

	VkSamplerCreateInfo samplerCreateInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
	samplerCreateInfo.minFilter  = VK_FILTER_LINEAR;
	samplerCreateInfo.magFilter  = VK_FILTER_LINEAR;
	samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerCreateInfo.maxLod     = FLT_MAX;

	VkExtent2D   imgSize    = pTexture->getSize();
	VkDeviceSize bufferSize = 0;
	VkFormat     format     = pTexture->getFormat();
	if (format == VK_FORMAT_R32G32B32A32_SFLOAT)
	{
		bufferSize = static_cast<uint64_t>(imgSize.width) * imgSize.height * 4 * sizeof(float);
	}
	else
	{
		bufferSize = static_cast<uint64_t>(imgSize.width) * imgSize.height * 4 * sizeof(uint8_t);
	}
	auto imageCreateInfo =
	    nvvk::makeImage2DCreateInfo(imgSize, format, VK_IMAGE_USAGE_SAMPLED_BIT, true);
	{
		nvvk::Image image =
		    m_alloc.createImage(cmdBuf, bufferSize, pTexture->getData(), imageCreateInfo);
		nvvk::cmdGenerateMipmaps(cmdBuf, image.image, format, imgSize,
		                         imageCreateInfo.mipLevels);
		VkImageViewCreateInfo ivInfo =
		    nvvk::makeImageViewCreateInfo(image.image, imageCreateInfo);
		m_texture = m_alloc.createTexture(image, ivInfo, samplerCreateInfo);
	}
}

void TextureAlloc::deinit(ContextAware *pContext)
{
	pContext->m_alloc.destroy(m_texture);
	intoReleased();
}