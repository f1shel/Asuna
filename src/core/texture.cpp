#include "texture.h"

#include <nvh/nvprint.hpp>
#include <nvvk/commands_vk.hpp>
#define STB_IMAGE_IMPLEMENTATION
#include <filesystem>
#include <stb_image.h>
using path = std::filesystem::path;

Texture::Texture()
{
  m_data   = (void*)malloc(1 * 1 * 4 * sizeof(float));
  m_format = VK_FORMAT_R32G32B32A32_SFLOAT;
  m_shape  = {(uint32_t)1, (uint32_t)1};
}

Texture::Texture(const std::string& texturePath, float gamma)
{
  static std::set<std::string> supportExtensions = {".hdr", ".jpg", ".png"};
  int32_t                      width{0};
  int32_t                      height{0};
  int32_t                      component{0};
  std::string                  ext = path(texturePath).extension().string();
  if(!supportExtensions.count(ext))
  {
    LOGE(
        "[x] %-20s: textures only support extensions (.hdr .jpg .png) while %s "
        "is passed in",
        "Scene Error", ext.c_str());
    exit(1);
  }
  VkFormat format = VK_FORMAT_UNDEFINED;
  void*    pixels = nullptr;
  // High dynamic range image
  if(ext == ".hdr")
  {
    format = VK_FORMAT_R32G32B32A32_SFLOAT;
    pixels = (void*)stbi_loadf(texturePath.c_str(), &width, &height, &component, STBI_rgb_alpha);
  }
  // 32bit image
  else
  {
    stbi_ldr_to_hdr_gamma(gamma);
    format = VK_FORMAT_R32G32B32A32_SFLOAT;
    pixels = (void*)stbi_loadf(texturePath.c_str(), &width, &height, &component, STBI_rgb_alpha);
    stbi_ldr_to_hdr_gamma(2.0);
  }
  // Handle failure
  if(!pixels)
  {
    LOGE("[x] %-20s: failed to load %s", "Scene Error", texturePath.c_str());
    exit(1);
  }
  m_data   = pixels;
  m_format = format;
  m_shape  = {(uint32_t)width, (uint32_t)height};
}

Texture::~Texture()
{
  m_shape  = {0};
  m_format = VK_FORMAT_UNDEFINED;
  if(m_data)
  {
    if(m_format == VK_FORMAT_R32G32B32A32_SFLOAT)
    {
      float* pixels = reinterpret_cast<float*>(m_data);
      stbi_image_free(pixels);
    }
    else
    {
      stbi_uc* pixels = reinterpret_cast<stbi_uc*>(m_data);
      stbi_image_free(pixels);
    }
  }
}

TextureAlloc::TextureAlloc(ContextAware* pContext, Texture* pTexture, const VkCommandBuffer& cmdBuf)
{
  auto& m_alloc = pContext->getAlloc();

  VkSamplerCreateInfo samplerCreateInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
  samplerCreateInfo.minFilter  = VK_FILTER_LINEAR;
  samplerCreateInfo.magFilter  = VK_FILTER_LINEAR;
  samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  samplerCreateInfo.maxLod     = FLT_MAX;

  VkExtent2D   imgSize    = pTexture->getSize();
  VkDeviceSize bufferSize = 0;
  VkFormat     format     = pTexture->getFormat();
  if(format == VK_FORMAT_R32G32B32A32_SFLOAT)
  {
    bufferSize = static_cast<uint64_t>(imgSize.width) * imgSize.height * 4 * sizeof(float);
  }
  else
  {
    bufferSize = static_cast<uint64_t>(imgSize.width) * imgSize.height * 4 * sizeof(uint8_t);
  }
  auto imageCreateInfo = nvvk::makeImage2DCreateInfo(imgSize, format, VK_IMAGE_USAGE_SAMPLED_BIT, true);
  {
    nvvk::Image image = m_alloc.createImage(cmdBuf, bufferSize, pTexture->getData(), imageCreateInfo);
    nvvk::cmdGenerateMipmaps(cmdBuf, image.image, format, imgSize, imageCreateInfo.mipLevels);
    VkImageViewCreateInfo ivInfo = nvvk::makeImageViewCreateInfo(image.image, imageCreateInfo);
    m_texture                    = m_alloc.createTexture(image, ivInfo, samplerCreateInfo);
  }
}

void TextureAlloc::deinit(ContextAware* pContext)
{
  pContext->getAlloc().destroy(m_texture);
  intoReleased();
}

EnvMap::EnvMap()
{
  m_data        = (void*)malloc(1 * 1 * 4 * sizeof(float));
  m_marginal    = (void*)malloc(1 * 1 * 4 * sizeof(float));
  m_conditional = (void*)malloc(1 * 1 * 4 * sizeof(float));
  m_shape       = {(uint32_t)1, (uint32_t)1};
}

EnvMap::EnvMap(const std::string& envmapPath)
{
  auto&                        textureName       = envmapPath;
  std::string                  ext               = std::filesystem::path(textureName).extension().string();
  static std::set<std::string> supportExtensions = {".hdr", ".jpg", ".png"};
  if(!supportExtensions.count(ext))
  {
    LOGE("[x] Scene loader: textures only support extensions (.hdr .jpg .png) while %s is passed in", ext.c_str());
    exit(1);
  }

  int32_t width{0};
  int32_t height{0};
  int32_t component{0};
  void*   pixels = nullptr;
  // High dynamic range image
  if(ext == ".hdr")
    pixels = (void*)stbi_loadf(textureName.c_str(), &width, &height, &component, STBI_rgb_alpha);
  // 32bit image
  else
  {
    stbi_ldr_to_hdr_gamma(1.0);
    pixels = (void*)stbi_loadf(textureName.c_str(), &width, &height, &component, STBI_rgb_alpha);
  }

  // Handle failure
  if(!pixels)
  {
    LOGE("[x] Scene loader: failed to load %s", textureName.c_str());
    exit(1);
  }
  m_data  = pixels;
  m_shape = {uint32_t(width), uint32_t(height)};


  // build acceleration lookup table for importance sampling
  vector<float> pdf2D(width * height);
  vector<float> cdf2D(width * height);
  vector<float> pdf1D(height);
  vector<float> cdf1D(height);

  m_marginal    = new nvmath::vec4f[width * height];
  m_conditional = new nvmath::vec4f[width * height];

  auto pixel       = static_cast<nvmath::vec4f*>(m_data);
  auto marginal    = static_cast<nvmath::vec4f*>(m_marginal);
  auto conditional = static_cast<nvmath::vec4f*>(m_conditional);

  float weightSum = 0.0f;
  for(int j = 0; j < height; j++)
  {
    float rowWeightSum = 0.0f;
    for(int i = 0; i < width; i++)
    {
      // luminance
      auto& color  = pixel[j * width + i];
      float weight = 0.3 * color.x + 0.6 * color.y + 0.1 * color.z;
      rowWeightSum += weight;
      pdf2D[j * width + i] = weight;
      cdf2D[j * width + i] = rowWeightSum;

      conditional[j * width + i] = nvmath::vec4f(0.f);
      marginal[j * width + i]    = nvmath::vec4f(0.f);
    }

    // Convert to range [0,1]
    for(int i = 0; i < width; i++)
    {
      pdf2D[j * width + i] /= rowWeightSum;
      cdf2D[j * width + i] /= rowWeightSum;
    }

    weightSum += rowWeightSum;

    pdf1D[j] = rowWeightSum;
    cdf1D[j] = weightSum;
  }

  // Convert to range [0,1]
  for(int j = 0; j < height; j++)
  {
    cdf1D[j] /= weightSum;
    pdf1D[j] /= weightSum;
  }

  auto lowerBound = [=](const float* array, int lower, int upper, const float value) {
    while(lower < upper)
    {
      int mid = (lower + upper) / 2;
      if(array[mid] < value)
        lower = mid + 1;
      else
        upper = mid;
    }
    return lower;
  };

  // Precalculate row and col to avoid binary search during lookup in the shader
  for(int j = 0; j < height; j++)
  {
    float invHeight       = static_cast<float>(j + 1) / height;
    float row             = lowerBound(cdf1D.data(), 0, height, invHeight);
    marginal[j * width].x = row / static_cast<float>(height);
    marginal[j * width].y = pdf1D[j];
  }

  for(int j = 0; j < height; j++)
  {
    for(int i = 0; i < width; i++)
    {
      float invWidth               = static_cast<float>(i + 1) / width;
      float col                    = lowerBound(cdf2D.data(), j * width, (j + 1) * width, invWidth) - j * width;
      conditional[j * width + i].x = col / static_cast<float>(width);
      conditional[j * width + i].y = pdf2D[j * width + i];
    }
  }
}

EnvMap::~EnvMap()
{
  m_shape = {0};
  if(m_data)
  {
    float* pixels = reinterpret_cast<float*>(m_data);
    stbi_image_free(pixels);
    float* marginal = reinterpret_cast<float*>(m_marginal);
    stbi_image_free(marginal);
    float* conditional = reinterpret_cast<float*>(m_conditional);
    stbi_image_free(conditional);
  }
}

EnvMapAlloc::EnvMapAlloc(ContextAware* pContext, EnvMap* pEnvmap, const VkCommandBuffer& cmdBuf)
{
  auto& m_alloc = pContext->getAlloc();

  VkSamplerCreateInfo samplerCreateInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
  samplerCreateInfo.minFilter  = VK_FILTER_LINEAR;
  samplerCreateInfo.magFilter  = VK_FILTER_LINEAR;
  samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  samplerCreateInfo.maxLod     = FLT_MAX;

  VkFormat     format     = pEnvmap->getFormat();
  VkExtent2D   imgSize    = pEnvmap->getSize();
  VkDeviceSize bufferSize = static_cast<uint64_t>(imgSize.width) * imgSize.height * 4 * sizeof(float);
  {
    auto        imageCreateInfo = nvvk::makeImage2DCreateInfo(imgSize, format, VK_IMAGE_USAGE_SAMPLED_BIT, true);
    nvvk::Image image           = m_alloc.createImage(cmdBuf, bufferSize, pEnvmap->getData(), imageCreateInfo);
    nvvk::cmdGenerateMipmaps(cmdBuf, image.image, format, imgSize, imageCreateInfo.mipLevels);
    VkImageViewCreateInfo ivInfo  = nvvk::makeImageViewCreateInfo(image.image, imageCreateInfo);
    nvvk::Texture         texture = m_alloc.createTexture(image, ivInfo, samplerCreateInfo);

    m_data = texture;
  }
  {
    auto        imageCreateInfo = nvvk::makeImage2DCreateInfo(imgSize, format, VK_IMAGE_USAGE_SAMPLED_BIT, true);
    nvvk::Image image           = m_alloc.createImage(cmdBuf, bufferSize, pEnvmap->getConditional(), imageCreateInfo);
    nvvk::cmdGenerateMipmaps(cmdBuf, image.image, format, imgSize, imageCreateInfo.mipLevels);
    VkImageViewCreateInfo ivInfo  = nvvk::makeImageViewCreateInfo(image.image, imageCreateInfo);
    nvvk::Texture         texture = m_alloc.createTexture(image, ivInfo, samplerCreateInfo);

    m_conditional = texture;
  }
  {
    auto        imageCreateInfo = nvvk::makeImage2DCreateInfo(imgSize, format, VK_IMAGE_USAGE_SAMPLED_BIT, true);
    nvvk::Image image           = m_alloc.createImage(cmdBuf, bufferSize, pEnvmap->getMarginal(), imageCreateInfo);
    nvvk::cmdGenerateMipmaps(cmdBuf, image.image, format, imgSize, imageCreateInfo.mipLevels);
    VkImageViewCreateInfo ivInfo  = nvvk::makeImageViewCreateInfo(image.image, imageCreateInfo);
    nvvk::Texture         texture = m_alloc.createTexture(image, ivInfo, samplerCreateInfo);

    m_marginal = texture;
  }
}

void EnvMapAlloc::deinit(ContextAware* pContext)
{
  auto& m_alloc = pContext->getAlloc();
  m_alloc.destroy(m_data);
  m_alloc.destroy(m_conditional);
  m_alloc.destroy(m_marginal);
  intoReleased();
}
