#ifndef STRUCTS_GLSL
#define STRUCTS_GLSL

struct Ray
{
    vec3 origin;
    vec3 direction;
};

struct BsdfSample
{
    vec3  bsdfDir;
    float pdf;
};

struct LightSample
{
    vec3  normal;
    vec3  position;
    vec3  emission;
    float pdf;
};

struct RayPayload
{
    uint       seed;
    uint       recur;
    bool       stop;
    Ray        ray;
    vec3       hit;
    vec3       radiance;
    vec3       throughput;
    BsdfSample bsdf;
};

#endif