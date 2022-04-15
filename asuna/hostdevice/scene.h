#ifndef SCENE_H
#define SCENE_H

#include "binding.h"

// Information of a obj model when referenced in a shader
struct MeshDesc
{
	// Address of the Vertex buffer
	uint64_t vertexAddress;
	// Address of the index buffer
	uint64_t indexAddress;
};

// SceneDesc = MeshDesc[]

#endif