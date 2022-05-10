#ifndef BINDING_H
#define BINDING_H

#ifdef __cplusplus
#    include <cstdint>
#    include "nvmath/nvmath.h"
using vec2 = nvmath::vec2f;
using vec3 = nvmath::vec3f;
using vec4 = nvmath::vec4f;
using mat4 = nvmath::mat4f;
using uint = unsigned int;
#endif

#ifdef __cplusplus        // Descriptor binding helper for C++ and GLSL
#    define START_BINDING(a) \
        enum a               \
        {
#    define END_BINDING() }
#else
#    define START_BINDING(a) const uint
#    define END_BINDING()
#endif

#ifdef __cplusplus        // glsl reserve enum keyword
#    define START_ENUM(a) \
        enum a            \
        {
#    define END_ENUM() }
#else
#    define START_ENUM(a) const uint
#    define END_ENUM()
#endif

START_ENUM(GPUSetGraphics)
eGPUSetGraphicsGraphics = 0, eGPUSetGraphicsCount = 1 END_ENUM();
START_ENUM(GPUSetRaytrace)
eGPUSetRaytraceGraphics = 0, eGPUSetRaytraceRaytrace = 1, eGPUSetRaytraceCount = 2 END_ENUM();
START_ENUM(GPUSetPost)
eGPUSetPostPost = 0, eGPUSetPostCount = 1 END_ENUM();

START_BINDING(GPUBindingGraphics)
eGPUBindingGraphicsCamera        = 0,        // Global uniform containing camera matrices
    eGPUBindingGraphicsSceneDesc = 1,        // Access to the scene descriptions
    eGPUBindingGraphicsTextures  = 2,        // Access to textures
    eGPUBindingGraphicsEmitters  = 3,        // Access to emitters
    eGPUBindingGraphicsSunAndSky = 4         // Access to sun_and_sky
    END_BINDING();

#define eGPUBindingRaytraceChannelCount 4
START_BINDING(GPUBindingRaytrace)
eGPUBindingRaytraceChannels = 0,        // Ray tracer channels
    eGPUBindingRaytraceTlas = 1         // Top level acceleration structure,
    END_BINDING();

START_BINDING(GPUBindingPost)
eGPUBindingPostImage = 0 END_BINDING();

#endif