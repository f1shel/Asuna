#include "texture.h"

#include <ImfRgba.h>
#include <ImfRgbaFile.h>
#include <shared/binding.h>
#include <filesystem/path.h>
using namespace filesystem;

#include <nvh/nvprint.hpp>
#include <nvvk/commands_vk.hpp>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

static float* readImageEXR(const std::string& name, int* width, int* height) {
  using namespace Imf;
  using namespace Imath;
  try {
    RgbaInputFile file(name.c_str());
    Box2i dw = file.dataWindow();
    Box2i dispw = file.displayWindow();

    // OpenEXR uses inclusive pixel bounds; adjust to non-inclusive
    // (the convention pbrt uses) in the values returned.
    *width = dw.max.x - dw.min.x + 1;
    *height = dw.max.y - dw.min.y + 1;

    std::vector<Rgba> pixels(*width * *height);
    file.setFrameBuffer(&pixels[0] - dw.min.x - dw.min.y * *width, 1, *width);
    file.readPixels(dw.min.y, dw.max.y);

    auto ret = reinterpret_cast<vec4*>(malloc(sizeof(vec4) * *width * *height));
    for (int i = 0; i < *width * *height; ++i) {
      ret[i].x = pixels[i].r;
      ret[i].y = pixels[i].g;
      ret[i].z = pixels[i].b;
      ret[i].w = pixels[i].a;
    }
    return reinterpret_cast<float*>(ret);
  } catch (const std::exception& e) {
    LOGE("[x] %-20s: failed to read image file %s: %s", "Scene Error",
         name.c_str(), e.what());
    exit(1);
  }

  return NULL;
}

static void writeImageEXR(const std::string& name, const float* pixels,
                          int xRes, int yRes, int totalXRes, int totalYRes,
                          int xOffset, int yOffset) {
  using namespace Imf;
  using namespace Imath;

  Rgba* hrgba = new Rgba[xRes * yRes];
  for (int i = 0; i < xRes * yRes; ++i)
    hrgba[i] = Rgba(pixels[4 * i], pixels[4 * i + 1], pixels[4 * i + 2],
                    pixels[4 * i + 3]);

  // OpenEXR uses inclusive pixel bounds.
  Box2i displayWindow(V2i(0, 0), V2i(totalXRes - 1, totalYRes - 1));
  Box2i dataWindow(V2i(xOffset, yOffset),
                   V2i(xOffset + xRes - 1, yOffset + yRes - 1));

  try {
    RgbaOutputFile file(name.c_str(), displayWindow, dataWindow, WRITE_RGB);
    file.setFrameBuffer(hrgba - xOffset - yOffset * xRes, 1, xRes);
    file.writePixels(yRes);
  } catch (const std::exception& exc) {
    printf("Error writing \"%s\": %s", name.c_str(), exc.what());
  }

  delete[] hrgba;
}

Texture::Texture() {
  m_data = (void*)malloc(1 * 1 * 4 * sizeof(float));
  m_format = VK_FORMAT_R32G32B32A32_SFLOAT;
  m_shape = {(uint32_t)1, (uint32_t)1};
}

Texture::Texture(const std::string& texturePath, float gamma) {
  int32_t width, height;
  m_data = readImage(texturePath, width, height, gamma);
  m_format = VK_FORMAT_R32G32B32A32_SFLOAT;
  m_shape = {(uint32_t)width, (uint32_t)height};
}

Texture::~Texture() {
  m_shape = {0};
  m_format = VK_FORMAT_UNDEFINED;
  if (m_data) {
    float* pixels = reinterpret_cast<float*>(m_data);
    stbi_image_free(pixels);
  }
}

TextureAlloc::TextureAlloc(ContextAware* pContext, Texture* pTexture,
                           const VkCommandBuffer& cmdBuf) {
  auto& m_alloc = pContext->getAlloc();

  VkSamplerCreateInfo samplerCreateInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
  samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
  samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
  samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  samplerCreateInfo.maxLod = FLT_MAX;

  VkExtent2D imgSize = pTexture->getSize();
  VkDeviceSize bufferSize = 0;
  VkFormat format = pTexture->getFormat();
  if (format == VK_FORMAT_R32G32B32A32_SFLOAT) {
    bufferSize = static_cast<uint64_t>(imgSize.width) * imgSize.height * 4 *
                 sizeof(float);
  } else {
    bufferSize = static_cast<uint64_t>(imgSize.width) * imgSize.height * 4 *
                 sizeof(uint8_t);
  }
  auto imageCreateInfo = nvvk::makeImage2DCreateInfo(
      imgSize, format, VK_IMAGE_USAGE_SAMPLED_BIT, true);
  {
    nvvk::Image image = m_alloc.createImage(
        cmdBuf, bufferSize, pTexture->getData(), imageCreateInfo);
    nvvk::cmdGenerateMipmaps(cmdBuf, image.image, format, imgSize,
                             imageCreateInfo.mipLevels);
    VkImageViewCreateInfo ivInfo =
        nvvk::makeImageViewCreateInfo(image.image, imageCreateInfo);
    m_texture = m_alloc.createTexture(image, ivInfo, samplerCreateInfo);
  }
}

void TextureAlloc::deinit(ContextAware* pContext) {
  pContext->getAlloc().destroy(m_texture);
  intoReleased();
}

EnvMap::EnvMap() {
  m_data = (void*)malloc(1 * 1 * 4 * sizeof(float));
  m_marginal = (void*)malloc(1 * 1 * 4 * sizeof(float));
  m_conditional = (void*)malloc(1 * 1 * 4 * sizeof(float));
  m_shape = {(uint32_t)1, (uint32_t)1};
}

EnvMap::EnvMap(const std::string& envmapPath) {
  int32_t width, height;
  m_data = readImage(envmapPath, width, height);
  m_shape = {uint32_t(width), uint32_t(height)};

  // build acceleration lookup table for importance sampling
  vector<float> pdf2D(width * height);
  vector<float> cdf2D(width * height);
  vector<float> pdf1D(height);
  vector<float> cdf1D(height);

  m_marginal = malloc(sizeof(nvmath::vec4f) * width * height);
  m_conditional = malloc(sizeof(nvmath::vec4f) * width * height);

  auto pixel = static_cast<nvmath::vec4f*>(m_data);
  auto marginal = static_cast<nvmath::vec4f*>(m_marginal);
  auto conditional = static_cast<nvmath::vec4f*>(m_conditional);

  float weightSum = 0.0f;
  for (int j = 0; j < height; j++) {
    float rowWeightSum = 0.0f;
    for (int i = 0; i < width; i++) {
      // luminance
      auto& color = pixel[j * width + i];
      float weight = 0.3 * color.x + 0.6 * color.y + 0.1 * color.z;
      rowWeightSum += weight;
      pdf2D[j * width + i] = weight;
      cdf2D[j * width + i] = rowWeightSum;

      conditional[j * width + i] = nvmath::vec4f(0.f);
      marginal[j * width + i] = nvmath::vec4f(0.f);
    }

    // Convert to range [0,1]
    for (int i = 0; i < width; i++) {
      pdf2D[j * width + i] /= (rowWeightSum + 1e-7);
      cdf2D[j * width + i] /= (rowWeightSum + 1e-7);
    }

    weightSum += rowWeightSum;

    pdf1D[j] = rowWeightSum;
    cdf1D[j] = weightSum;
  }

  // Convert to range [0,1]
  for (int j = 0; j < height; j++) {
    cdf1D[j] /= (weightSum + 1e-7);
    pdf1D[j] /= (weightSum + 1e-7);
  }

  auto lowerBound = [=](const float* array, int lower, int upper,
                        const float value) {
    while (lower < upper) {
      int mid = (lower + upper) / 2;
      if (array[mid] < value)
        lower = mid + 1;
      else
        upper = mid;
    }
    return lower;
  };

  // Precalculate row and col to avoid binary search during lookup in the
  // shader
  for (int j = 0; j < height; j++) {
    float invHeight = static_cast<float>(j + 1) / height;
    float row = lowerBound(cdf1D.data(), 0, height, invHeight);
    marginal[j * width].x = row / static_cast<float>(height);
    marginal[j * width].y = pdf1D[j];
  }

  for (int j = 0; j < height; j++) {
    for (int i = 0; i < width; i++) {
      float invWidth = static_cast<float>(i + 1) / width;
      float col =
          lowerBound(cdf2D.data(), j * width, (j + 1) * width, invWidth) -
          j * width;
      conditional[j * width + i].x = col / static_cast<float>(width);
      conditional[j * width + i].y = pdf2D[j * width + i];
    }
  }
}

EnvMap::~EnvMap() {
  m_shape = {0};
  if (m_data) {
    float* pixels = reinterpret_cast<float*>(m_data);
    stbi_image_free(pixels);
    float* marginal = reinterpret_cast<float*>(m_marginal);
    stbi_image_free(marginal);
    float* conditional = reinterpret_cast<float*>(m_conditional);
    stbi_image_free(conditional);
  }
}

EnvMapAlloc::EnvMapAlloc(ContextAware* pContext, EnvMap* pEnvmap,
                         const VkCommandBuffer& cmdBuf) {
  auto& m_alloc = pContext->getAlloc();

  VkSamplerCreateInfo samplerCreateInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
  samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
  samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
  samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  samplerCreateInfo.maxLod = FLT_MAX;

  VkFormat format = pEnvmap->getFormat();
  VkExtent2D imgSize = pEnvmap->getSize();
  VkDeviceSize bufferSize =
      static_cast<uint64_t>(imgSize.width) * imgSize.height * 4 * sizeof(float);
  {
    auto imageCreateInfo = nvvk::makeImage2DCreateInfo(
        imgSize, format, VK_IMAGE_USAGE_SAMPLED_BIT, true);
    nvvk::Image image = m_alloc.createImage(
        cmdBuf, bufferSize, pEnvmap->getData(), imageCreateInfo);
    nvvk::cmdGenerateMipmaps(cmdBuf, image.image, format, imgSize,
                             imageCreateInfo.mipLevels);
    VkImageViewCreateInfo ivInfo =
        nvvk::makeImageViewCreateInfo(image.image, imageCreateInfo);
    nvvk::Texture texture =
        m_alloc.createTexture(image, ivInfo, samplerCreateInfo);

    m_data = texture;
  }
  {
    auto imageCreateInfo = nvvk::makeImage2DCreateInfo(
        imgSize, format, VK_IMAGE_USAGE_SAMPLED_BIT, true);
    nvvk::Image image = m_alloc.createImage(
        cmdBuf, bufferSize, pEnvmap->getConditional(), imageCreateInfo);
    nvvk::cmdGenerateMipmaps(cmdBuf, image.image, format, imgSize,
                             imageCreateInfo.mipLevels);
    VkImageViewCreateInfo ivInfo =
        nvvk::makeImageViewCreateInfo(image.image, imageCreateInfo);
    nvvk::Texture texture =
        m_alloc.createTexture(image, ivInfo, samplerCreateInfo);

    m_conditional = texture;
  }
  {
    auto imageCreateInfo = nvvk::makeImage2DCreateInfo(
        imgSize, format, VK_IMAGE_USAGE_SAMPLED_BIT, true);
    nvvk::Image image = m_alloc.createImage(
        cmdBuf, bufferSize, pEnvmap->getMarginal(), imageCreateInfo);
    nvvk::cmdGenerateMipmaps(cmdBuf, image.image, format, imgSize,
                             imageCreateInfo.mipLevels);
    VkImageViewCreateInfo ivInfo =
        nvvk::makeImageViewCreateInfo(image.image, imageCreateInfo);
    nvvk::Texture texture =
        m_alloc.createTexture(image, ivInfo, samplerCreateInfo);

    m_marginal = texture;
  }
}

void EnvMapAlloc::deinit(ContextAware* pContext) {
  auto& m_alloc = pContext->getAlloc();
  m_alloc.destroy(m_data);
  m_alloc.destroy(m_conditional);
  m_alloc.destroy(m_marginal);
  intoReleased();
}

float* readImage(const std::string& imagePath, int& width, int& height,
                 float gamma) {
  static std::set<std::string> supportExtensions = {"hdr", "exr", "jpg", "png"};
  std::string ext = path(imagePath).extension();
  if (!supportExtensions.count(ext)) {
    LOG_ERROR(
        "{}: textures only support extensions (hdr exr jpg png) "
        "while [{}] "
        "is passed in",
        "Scene", ext);
    exit(1);
  }
  void* pixels = nullptr;
  // High dynamic range image
  if (ext == "hdr")
    pixels = (void*)stbi_loadf(imagePath.c_str(), &width, &height, nullptr,
                               STBI_rgb_alpha);
  else if (ext == "exr")
    pixels = readImageEXR(imagePath, &width, &height);
  // 32bit image
  else {
    stbi_ldr_to_hdr_gamma(gamma);
    pixels = (void*)stbi_loadf(imagePath.c_str(), &width, &height, nullptr,
                               STBI_rgb_alpha);
    stbi_ldr_to_hdr_gamma(stbi__l2h_gamma);
  }
  // Handle failure
  if (!pixels) {
    LOGE("[x] %-20s: failed to load %s", "Scene Error", imagePath.c_str());
    exit(1);
  }
  return reinterpret_cast<float*>(pixels);
}

void writeImage(const std::string& imagePath, int width, int height,
                float* data) {
  static std::set<std::string> supportExtensions = {"hdr", "exr", "jpg",
                                                    "png", "tga", "bmp"};
  std::string ext = path(imagePath).extension();
  if (!supportExtensions.count(ext)) {
    LOG_ERROR(
        "{}: textures only support extensions (hdr exr jpg png) "
        "while [{}] "
        "is passed in",
        "Scene Error", ext);
    exit(1);
  }
  if (ext == "hdr")
    stbi_write_hdr(imagePath.c_str(), width, height, 4, data);
  else if (ext == "exr")
    writeImageEXR(imagePath, data, width, height, width, height, 0, 0);
  else {
    stbi_hdr_to_ldr_gamma(1.0);
    auto autoDestroyData = reinterpret_cast<float*>(
        STBI_MALLOC(width * height * 4 * sizeof(float)));
    memcpy(autoDestroyData, data, width * height * 4 * sizeof(float));
    auto ldrData = stbi__hdr_to_ldr(autoDestroyData, width, height, 4);
    if (ext == "jpg")
      stbi_write_jpg(imagePath.c_str(), width, height, 4, ldrData, 0);
    else if (ext == "png")
      stbi_write_png(imagePath.c_str(), width, height, 4, ldrData, 0);
    else if (ext == "tga")
      stbi_write_tga(imagePath.c_str(), width, height, 4, ldrData);
    else if (ext == "bmp")
      stbi_write_bmp(imagePath.c_str(), width, height, 4, ldrData);
    stbi_hdr_to_ldr_gamma(stbi__h2l_gamma_i);
  }
}
