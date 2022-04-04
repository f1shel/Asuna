#pragma once

#include "context.h"
#include "pipeline.h"

class Tracer {
public:
    void init();
    void run();
    void deinit();
private:
    ContextAware m_context;

};