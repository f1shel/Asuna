#ifndef ASUNA_BINDING_H
#define ASUNA_BINDING_H

#ifdef __cplusplus
#include "nvmath/nvmath.h"
using vec2 = nvmath::vec2f;
using vec3 = nvmath::vec3f;
using vec4 = nvmath::vec4f;
using mat4 = nvmath::mat4f;
using uint = unsigned int;
#endif

#ifdef __cplusplus // Descriptor binding helper for C++ and GLSL
#define START_BINDING(a) enum a {
#define END_BINDING() }
#else
#define START_BINDING(a)  const uint
#define END_BINDING() 
#endif

#ifdef __cplusplus // glsl reserve enum keyword
#define START_ENUM(a) enum a {
#define END_ENUM() }
#else
#define START_ENUM(a)  const uint
#define END_ENUM() 
#endif

START_BINDING(BindingsGraphic)
eBindingGraphicCamera = 0 // Global uniform containing camera matrices
END_BINDING();

START_BINDING(BindingsRaytrace)
//eBindingRaytraceTlas = 0, // Top level acceleration structure,
eBindingRaytraceImage = 0 // Ray tracer image
END_BINDING();

START_BINDING(BindingsPost)
eBindingPostImage = 0
END_BINDING();


#endif