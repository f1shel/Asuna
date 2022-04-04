#pragma once

#include "context/context.h"
#include "pipeline/pipeline_graphic.h"

class Tracer {
public:
    void init();
    void run();
    void deinit();
private:
    // context
    ContextAware m_context;
    // pipelines
    PipelineGraphic m_pipelineGraphic;
};