#ifndef MATH_GLSL
#define MATH_GLSL

#define PI 3.14159265358979323846
#define TWO_PI 6.28318530717958647692
#define INV_PI 0.31830988618379067154
#define INV_2PI 0.15915494309189533577

#define EPS 0.001
#define INFINITY 10000000000.0
#define MINIMUM 0.00001

void initRNG(vec2 p, int frame, inout uvec4 seed)
{
  seed = uvec4(p, uint(frame), uint(p.x) + uint(p.y));
}

void pcg4d(inout uvec4 v)
{
  v = v * 1664525u + 1013904223u;
  v.x += v.y * v.w;
  v.y += v.z * v.x;
  v.z += v.x * v.y;
  v.w += v.y * v.z;
  v = v ^ (v >> 16u);
  v.x += v.y * v.w;
  v.y += v.z * v.x;
  v.z += v.x * v.y;
  v.w += v.y * v.z;
}

float rand(inout uvec4 seed)
{
  pcg4d(seed);
  return float(seed.x) / float(0xffffffffu);
}


/*
 * Generate a random seed for the random generator.
 *
 * Note:
 *  (1) Input: three given unsigned integers
 *  (2) Reference:
 *      https://github.com/Cyan4973/xxHash
 *      https://www.shadertoy.com/view/XlGcRh
 */
uint xxhash32(uvec3 p)
{
  const uvec4 primes = uvec4(2246822519U, 3266489917U, 668265263U, 374761393U);
  uint        h32;
  h32 = p.z + primes.w + p.x * primes.y;
  h32 = primes.z * ((h32 << 17) | (h32 >> (32 - 17)));
  h32 += p.y * primes.y;
  h32 = primes.z * ((h32 << 17) | (h32 >> (32 - 17)));
  h32 = primes.x * (h32 ^ (h32 >> 15));
  h32 = primes.y * (h32 ^ (h32 >> 13));
  return h32 ^ (h32 >> 16);
}

/*
 * PCG, A Family of Better Random Number Generators
 *
 * Note:
 *  (1) Reference:
 *      http://www.pcg-random.org
 */
uint pcg(inout uint state)
{
  uint prev = state * 747796405u + 2891336453u;
  uint word = ((prev >> ((prev >> 28u) + 4u)) ^ prev) * 277803737u;
  state     = prev;
  return (word >> 22u) ^ word;
}

/*
 * Generate a random float in [0, 1) given the previous RNG state
 */
// float rand(inout uint seed)
// {
//   uint r = pcg(seed);
//   return r * (1.0 / float(0xffffffffu));
// }

/*
 * Uniformly sample vector on the unit sphere surface
 *
 * Note:
 *  (1) Reference:
 *      https://www.pbr-book.org/3ed-2018/Monte_Carlo_Integration/2D_Sampling_with_Multidimensional_Transformations
 *
 * Return:
 *  A 4d vector
 *      xyz: sampled vector
 *      w: sampled pdf
 */
vec3 uniformSampleSphere(inout uvec4 seed, inout float pdf)
{
  float r1 = rand(seed);
  float r2 = rand(seed);

  float z   = 1.0 - 2.0 * r1;
  float r   = sqrt(max(0.f, 1.0 - z * z));
  float phi = 2.0 * PI * r2;

  float x = r * cos(phi);
  float y = r * sin(phi);

  pdf = 0.25 * INV_PI;

  return vec3(x, y, z);
}

/*
 * Uniformly sample vector on the unit hemisphere surface
 *
 * Note:
 *  (1) Reference:
 *      https://www.pbr-book.org/3ed-2018/Monte_Carlo_Integration/2D_Sampling_with_Multidimensional_Transformations
 *
 * Return:
 *  A 4d vector
 *      xyz: sampled vector
 *      w: sampled pdf
 */
vec4 uniformSampleHemisphere(inout uvec4 seed)
{
  float r1 = rand(seed);
  float r2 = rand(seed);

  float r   = sqrt(max(0, 1.0 - r1 * r1));
  float phi = 2 * PI * r2;

  return vec4(r * cos(phi), r * sin(phi), r1, 0.5 * INV_PI);
}

/*
 * Cosine weighted sample vector on the unit hemisphere surface
 *
 * Note:
 *  (1) Reference:
 *      https://www.pbr-book.org/3ed-2018/Monte_Carlo_Integration/2D_Sampling_with_Multidimensional_Transformations
 *
 * Return:
 *  A 4d vector
 *      xyz: sampled vector
 *      w: sampled pdf
 */
vec3 cosineSampleHemisphere(inout uvec4 seed, inout float pdf)
{
  float r1 = rand(seed);
  float r2 = rand(seed);

  vec3  dir;
  float r   = sqrt(r1);
  float phi = 2.0 * PI * r2;

  dir.x = r * cos(phi);
  dir.y = r * sin(phi);
  dir.z = sqrt(max(0.0, 1.0 - dir.x * dir.x - dir.y * dir.y));

  pdf = dir.z * INV_PI;

  return vec3(dir.x, dir.y, dir.z);
}

/*
 * Power heuristic often reduces variance even further for multiple importance
 * sampling
 *
 * NOTE:
 *  (1) Reference:
 *      https://pbr-book.org/3ed-2018/Monte_Carlo_Integration/Importance_Sampling
 */
float powerHeuristic(float a, float b)
{
  float t = a * a;
  return t / (b * b + t);
}

/*
 * Normal to orthonormal basis using quaternion similarity
 *
 * NOTE:
 *  (1) Reference:
 *      https://www.shadertoy.com/view/lldGRM
 */
#define HANDLE_SINGULARITY
void basis(in vec3 n, out vec3 f, out vec3 r)
{
#ifdef HANDLE_SINGULARITY
  if(n.z < -0.999999)
  {
    f = vec3(0, -1, 0);
    r = vec3(-1, 0, 0);
  }
  else
  {
    float a = 1. / (1. + n.z);
    float b = -n.x * n.y * a;
    f       = vec3(1. - n.x * n.x * a, b, -n.x);
    r       = vec3(b, 1. - n.y * n.y * a, -n.y);
  }
#else
  float a = 1. / (1. + n.z);
  float b = -n.x * n.y * a;
  f       = vec3(1. - n.x * n.x * a, b, -n.x);
  r       = vec3(b, 1. - n.y * n.y * a, -n.y);
#endif
}

/*
 * offsetPositionAlongNormal shifts a point on a triangle surface so that a
 * ray bouncing off the surface with tMin = 0.0 is no longer treated as
 * intersecting the surface it originated from.
 *
 * Here's the old implementation of it we used in earlier chapters:
 * vec3 offsetPositionAlongNormal(vec3 worldPosition, vec3 normal)
 * {
 *   return worldPosition + 0.0001 * normal;
 * }
 *
 * However, this code uses an improved technique by Carsten W?chter and
 * Nikolaus Binder from "A Fast and Robust Method for Avoiding
 * Self-Intersection" from Ray Tracing Gems (verion 1.7, 2020).
 * The normal can be negated if one wants the ray to pass through
 * the surface instead.
 */
vec3 offsetPositionAlongNormal(vec3 worldPosition, vec3 normal)
{
  // Convert the normal to an integer offset.
  const float int_scale = 256.0f;
  const ivec3 of_i      = ivec3(int_scale * normal);

  // Offset each component of worldPosition using its binary representation.
  // Handle the sign bits correctly.
  const vec3 p_i = vec3(  //
      intBitsToFloat(floatBitsToInt(worldPosition.x) + ((worldPosition.x < 0) ? -of_i.x : of_i.x)),
      intBitsToFloat(floatBitsToInt(worldPosition.y) + ((worldPosition.y < 0) ? -of_i.y : of_i.y)),
      intBitsToFloat(floatBitsToInt(worldPosition.z) + ((worldPosition.z < 0) ? -of_i.z : of_i.z)));

  // Use a floating-point offset instead for points near (0,0,0), the origin.
  const float origin     = 1.0f / 32.0f;
  const float floatScale = 1.0f / 65536.0f;
  return vec3(  //
      abs(worldPosition.x) < origin ? worldPosition.x + floatScale * normal.x : p_i.x,
      abs(worldPosition.y) < origin ? worldPosition.y + floatScale * normal.y : p_i.y,
      abs(worldPosition.z) < origin ? worldPosition.z + floatScale * normal.z : p_i.z);
}

#endif