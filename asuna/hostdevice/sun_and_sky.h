#pragma once

#ifndef SUN_AND_SKY_H
#    define SUN_AND_SKY_H

#include "binding.h"

struct SunAndSky
{
    vec3  rgb_unit_conversion;
    float multiplier;

    float haze;
    float redblueshift;
    float saturation;
    float horizon_height;

    vec3  ground_color;
    float horizon_blur;

    vec3  night_color;
    float sun_disk_intensity;

    vec3  sun_direction;
    float sun_disk_scale;

    float sun_glow_intensity;
    int   y_is_up;
    int   physically_scaled_sun;
    int   in_use;
};

#endif