#ifndef MATH_GLSL
#define MATH_GLSL

#define PI 3.14159265358979323846
#define SQRT_2 1.41421356237309504880
#define TWO_PI 6.28318530717958647692
#define INV_PI 0.31830988618379067154
#define INV_2PI 0.15915494309189533577
#define INV_4PI 0.07957747154594766788
#define PI_OVER_2 1.57079632679489661923
#define PI_OVER_4 0.78539816339744830961

#define EPS 0.001
#define INFINITY 10000000000.0
#define MINIMUM 0.00001

/*
 * Generate a random seed for the random generator.
 *
 * Note:
 *  (1) Input: three given unsigned integers
 *  (2) Reference:
 *      https://github.com/Cyan4973/xxHash
 *      https://www.shadertoy.com/view/XlGcRh
 */
uint xxhash32Seed(uvec3 p)
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
float rand(inout uint seed)
{
  uint r = pcg(seed);
  return r * (1.0 / float(0xffffffffu));
}

vec3 uniformSampleSphere(in vec2 u)
{
  float z   = 1 - 2 * u.x;
  float r   = sqrt(max(0, 1 - z * z));
  float phi = TWO_PI * u.y;
  return vec3(r * cos(phi), r * sin(phi), z);
}

float uniformSpherePdf()
{
  return INV_4PI;
}

vec2 uniformSampleDisk(in vec2 u)
{
  float r     = sqrt(u.x);
  float theta = TWO_PI * u.y;
  return vec2(r * cos(theta), r * sin(theta));
}

vec2 concentricSampleDisk(in vec2 u)
{
  // Map uniform random numbers to [-1, 1]^2
  vec2 uOffset = 2.f * u - 1.f;
  // Handle degeneracy at the origin
  if(uOffset.x == 0 && uOffset.y == 0)
    return vec2(0, 0);
  // Apply concentric mapping to point
  float theta, r;
  if(abs(uOffset.x) > abs(uOffset.y))
  {
    r     = uOffset.x;
    theta = PI_OVER_4 * (uOffset.y / uOffset.x);
  }
  else
  {
    r     = uOffset.y;
    theta = PI_OVER_2 - PI_OVER_4 * (uOffset.x / uOffset.y);
  }
  return r * vec2(cos(theta), sin(theta));
}

vec3 cosineSampleHemisphere(in vec2 u)
{
  vec2  d = concentricSampleDisk(u);
  float z = sqrt(max(0, 1 - d.x * d.x - d.y * d.y));
  return vec3(d.x, d.y, z);
}

float cosineHemispherePdf(in float cosTheta)
{
  return cosTheta * INV_PI;
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