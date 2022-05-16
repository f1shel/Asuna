#ifndef MATERIAL_H
#define MATERIAL_H

#include "binding.h"

struct GpuMaterial
{
    vec3 diffuse;                   // diffuse albedo
    vec3 specular;                  // specular albedo
    vec2 axay;                      // alpha x and y
    vec2 roughness;                 // roughness x and y
    int  diffuseTextureId;          // diffuse albedo texture id
    int  specularTextureId;         // specular albedo texture id
    int  alphaTextureId;            // alpha texture id
    int  roughnessTextureId;        // roughness texture id
    int  normalTextureId;           // normal texture id
    int  tangentTextureId;          // tangent texture id
};

#endif