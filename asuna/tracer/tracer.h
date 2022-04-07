#pragma once

#include "context/context.h"
#include "scene/scene.h"
#include "pipeline/pipeline_graphic.h"
#include "pipeline/pipeline_raytrace.h"
#include "pipeline/pipeline_post.h"

class Tracer {
public:
    void init();
    void run();
    void deinit();
private:
    // context
    ContextAware m_context;
    // scene
    SceneAware m_scene;
    // pipelines
    PipelineGraphic m_pipelineGraphic;
    PipelineRaytrace m_pipelineRaytrace;
    PipelinePost m_pipelinePost;
};