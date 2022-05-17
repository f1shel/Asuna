#include "camera.h"

/**
 * Illustration of coordinate systems:
 * 
 * (1) Screen space: Screen space is defined on the film plane.
 * The camera projects objects in camera space onto the film
 * plane; the parts inside the screen window are visible in the
 * image that is generated. Depth z values in screen space range
 * from 0 to 1, corresponding to points at the near and far
 * clipping planes, respectively. Note that, although this is
 * called "screen" space, it is still a 3D coordinate system,
 * since z values are meaningful. In x and y, this space ranges
 * from (-1,-1) to (1,1).
 * 
 * (2) Normalized device coordinate (NDC) space: This is the
 * coordinate system for the actual image being rendered. In x and
 * y, this space ranges from (0,0) to (1,1), with (0,) being the
 * upper-left corner of the image. Depth values are the same as in
 * screen space, and a linear transformation converts from screen
 * to NDC space.
 * 
 * (3) Raster space: This is almost the same as NDC space, except
 * the x and y coordinates range from (0,0) to (width, height).
 */

mat4 perspectiveTransform(float fov, float nearz, float farz)
{
  mat4  persp{nvmath::mat4f_zero};
  float recip = 1.0f / (farz - nearz);
  /* Perform a scale so that the field of view is mapped
     * to the interval [-1, 1] */
  float ctot = 1.0 / tanf(fov * nv_to_rad * 0.5);
  persp.a00  = ctot;
  persp.a11  = ctot;
  persp.a22  = farz * recip;
  persp.a32  = 1;
  persp.a33  = -nearz * farz * recip;
  return persp;
}

mat4 cameraToRasterTransform(VkExtent2D filmSize, float fov, float near, float far)
{
  float aspect    = filmSize.width / float(filmSize.height);
  float invAspect = 1.f / aspect;
  // This gives a transformation from camera space to "screen" space.
  // In x this space ranges from -1 to 1 but y ranges from -invAspect
  // to invAspect, since aspect ratio is not taken into account.
  mat4 cameraToScreen = perspectiveTransform(fov, near, far);
  // x and y ranges from (-1,-1) to (1,1)
  cameraToScreen = nvmath::scale_mat4(vec3(1.f, aspect, 1.f)) * cameraToScreen;
  // x and y ranges from (0,0) to (1,1)
  mat4 cameraToNdc = nvmath::scale_mat4(vec3(-0.5f, -0.5f, 1.f)) * nvmath::translation_mat4(vec3(-1.f, -1.f, 0.f)) * cameraToScreen;
  // x and y ranges from (0,0) to (width,height)
  mat4 cameraToRaster = nvmath::scale_mat4(vec3(1.f / filmSize.width, 1.f / filmSize.height, 1.f)) * cameraToNdc;
  return cameraToRaster;
}

void Camera::setToWorld(const vec3& lookat, const vec3& eye, const vec3& up)
{
  CameraManip.setLookat(eye, lookat, up);
  m_view = CameraManip.getMatrix();
}

void Camera::setToWorld(CameraShot shot)
{
  if(shot.ext.a33 == 1.f)
  {
    m_view = shot.ext;
    // todo: set CameraManip's eye up lookat
  }
  else
    setToWorld(shot.lookat, shot.eye, shot.up);
}

void Camera::adaptFilm()
{
  CameraManip.setWindowSize(m_size.width, m_size.height);
}

GpuCamera CameraOpencv::toGpuStruct()
{
  static GpuCamera cam;
  cam.type          = getType();
  cam.cameraToWorld = nvmath::invert_rot_trans(getView());
  cam.fxfycxcy      = m_fxfycxcy;
  return cam;
}

GpuCamera CameraPerspective::toGpuStruct()
{
  static float     nearz = 0.1f, farz = 100.0f;
  static GpuCamera cam;
  cam.type          = getType();
  cam.cameraToWorld = nvmath::invert_rot_trans(getView());
  cam.rasterToCamera = nvmath::invert(cameraToRasterTransform(getFilmSize(), getFov(), 0.1, 100.f));
  return cam;
}