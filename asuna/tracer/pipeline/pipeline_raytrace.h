#pragma once

#include "../../hostdevice/pushconstant.h"
#include "pipeline.h"
#include "pipeline_graphic.h"

#include "nvvk/raytraceKHR_vk.hpp"
#include "nvvk/sbtwrapper_vk.hpp"

class PipelineCorrelatedRaytrace : public PipelineCorrelated
{
  public:
	PipelineGraphic *m_pPipGraphic = nullptr;
};

class PipelineRaytrace : public PipelineAware
{
  public:
	virtual void init(PipelineCorrelated *pPipCorr);
	virtual void deinit();
	virtual void run(const VkCommandBuffer &cmdBuf);

  private:
	// Request ray tracing pipeline properties
	void initRayTracing();
	// Create bottom level acceleration structures
	void createBottomLevelAS();
	// Create top level acceleration structures
	void createTopLevelAS();
	void createRtDescriptorSetLayout();
	// Create ray tracing pipeline
	void createRtPipeline();
	// Update the descriptor pointer
	void updateRtDescriptorSet();

  private:
	// Accompanied graphic pipeline
	PipelineGraphic *m_pPipGraphic = nullptr;
	// Pipeline properties
	VkPhysicalDeviceRayTracingPipelinePropertiesKHR m_rtProperties = {
	    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR};
	// Shading binding table wrapper
	nvvk::SBTWrapper m_sbt;
	// Pipeline builder
	nvvk::RaytracingBuilderKHR m_rtBuilder;
	// Top level acceleration structures
	std::vector<VkAccelerationStructureInstanceKHR> m_tlas{};
	// Bottom level acceleration structures
	std::vector<nvvk::RaytracingBuilderKHR::BlasInput> m_blas{};
	// Push constant
	PushConstantRaytrace m_pcRaytrace{0};
};