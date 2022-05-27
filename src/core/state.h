#pragma once

#include <shared/pushconstant.h>

class State
{
public:
  GpuPushConstantGraphics graphicsState;
  GpuPushConstantRaytrace rtxState;
  GpuPushConstantPost     postState;

  State()
  {
    graphicsState.placeholder = 0;

    rtxState.curFrame       = -1;         // rewrite by procedural rendering
    rtxState.spp            = 1;          // rewrite by loader::parse()
    rtxState.maxPathDepth   = 3;          // rewrite by loader::parse()
    rtxState.numLights      = 0;          // rewrite by loader::submit()
    rtxState.useFaceNormal  = 0;          // rewrite by loader::parse()
    rtxState.ignoreEmissive = 0;          // rewrite by loader::parse()
    rtxState.hasEnvMap      = 0;          // rewrite by loader::submit()
    rtxState.bgColor        = vec3(0.f);  // rewrite by loader::parse()

    postState.brightness     = 1.f;                    // rewrite by gui
    postState.contrast       = 1.f;                    // rewrite by gui
    postState.saturation     = 1.f;                    // rewrite by gui
    postState.vignette       = 0.f;                    // rewrite by gui
    postState.avgLum         = 1.f;                    // rewrite by gui
    postState.zoom           = 1.f;                    // rewrite by gui
    postState.renderingRatio = {1.f, 1.f};             // rewrite by gui
    postState.autoExposure   = 0;                      // rewrite by gui
    postState.Ywhite         = 0.5f;                   // rewrite by gui
    postState.key            = 0.5f;                   // rewrite by gui
    postState.tmType         = ToneMappingTypeFilmic;  // rewrite by loader::parse() & gui
  }
};