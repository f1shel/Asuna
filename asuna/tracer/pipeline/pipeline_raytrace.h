#pragma once

#include "../../hostdevice/pushconstant.h"
#include "pipeline.h"
#include "pipeline_graphics.h"
#include "nvvk/raytraceKHR_vk.hpp"
#include "nvvk/sbtwrapper_vk.hpp"

struct PipelineRaytraceInitState
{
  DescriptorSetWrapper* pDswOut   = nullptr;
  DescriptorSetWrapper* pDswScene = nullptr;
  DescriptorSetWrapper* pDswEnv   = nullptr;
};

class PipelineRaytrace : public PipelineAware
{
public:
  enum class HoldSet
  {
    Accel = 0,
    Num   = 1,
  };
  PipelineRaytrace()
      : PipelineAware(uint(HoldSet::Num), RtBindSet::RtNum)
  {
  }
  virtual void             init(ContextAware* pContext, Scene* pScene, PipelineRaytraceInitState& pis);
  virtual void             deinit();
  virtual void             run(const VkCommandBuffer& cmdBuf);
  GpuPushConstantRaytrace& getPushconstant() { return m_pushconstant; }
  void                     setSpp(int spp = 1);
  void                     resetFrame();
  void                     incrementFrame();

private:
  void initRayTracing();               // Request ray tracing pipeline properties
  void createBottomLevelAS();          // Create bottom level acceleration structures
  void createTopLevelAS();             // Create top level acceleration structures
  void createRtDescriptorSetLayout();  // Create descriptor sets
  void createRtPipeline();             // Create ray tracing pipeline
  void updateRtDescriptorSet();        // Update the descriptor pointer

private:
  nvvk::SBTWrapper                              m_sbt;              // Shading binding table wrapper
  nvvk::RaytracingBuilderKHR                    m_rtBuilder;        // Pipeline builder
  vector<VkAccelerationStructureInstanceKHR>    m_tlas{};           // Top level acceleration structures
  vector<nvvk::RaytracingBuilderKHR::BlasInput> m_blas{};           // Bottom level acceleration structures
  GpuPushConstantRaytrace                       m_pushconstant{0};  // Push constant
};