#pragma once

#include <shared/pushconstant.h>

class State {
public:
  GpuPushConstantGraphics graphicsState;
  GpuPushConstantRaytrace rtxState;
  GpuPushConstantPost postState;

  State() {
    graphicsState.placeholder = 0;

    // rewrite by procedural rendering
    rtxState.curFrame = -1;
    // rewrite by Loader::parse()
    rtxState.spp = 1;
    rtxState.maxPathDepth = 3;
    // rewrite by Loader::submit()
    rtxState.numLights = 0;
    // rewrite by Loader::parse()
    rtxState.useFaceNormal = 0;
    rtxState.ignoreEmissive = 0;
    // rewrite by Scene::addEnvMap()
    rtxState.hasEnvMap = 0;
    // rewrite by Loader::parse()
    rtxState.envMapIntensity = 1.f;
    // rewrite by Loader::submit()
    rtxState.envMapResolution = vec2(0.f);
    // rewrite by Loader::parse()
    rtxState.bgColor = vec3(0.f);
    rtxState.envRotateAngle = 0.f;

    // rewrite by gui
    postState.brightness = 1.f;
    postState.contrast = 1.f;
    postState.saturation = 1.f;
    postState.vignette = 0.f;
    postState.avgLum = 1.f;
    postState.zoom = 1.f;
    postState.renderingRatio = {1.f, 1.f};
    postState.autoExposure = 0;
    postState.Ywhite = 0.5f;
    postState.key = 0.5f;
    // rewrite by Loader::parse() & gui
    postState.tmType = ToneMappingTypeFilmic;
  }
};