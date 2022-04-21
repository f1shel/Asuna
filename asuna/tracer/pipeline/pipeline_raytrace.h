#pragma once

#include "../../hostdevice/pushconstant.h"
#include "pipeline.h"
#include "pipeline_graphics.h"

#include "nvvk/raytraceKHR_vk.hpp"
#include "nvvk/sbtwrapper_vk.hpp"

class PipelineRaytrace : public PipelineAware
{
  public:
	virtual void init(PipelineInitState pis);
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
	PipelineGraphics *m_pPipGraphics = nullptr;
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
	GPUPushConstantRaytrace m_pcRaytrace{0};
};