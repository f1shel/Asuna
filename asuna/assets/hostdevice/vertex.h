#ifndef VERTEX_H
#define VERTEX_H

#include "binding.h"

struct Vertex
{
    vec3 position;
    vec2 texCoord;
    vec3 normal;
    vec3 tangent;
};

#endif