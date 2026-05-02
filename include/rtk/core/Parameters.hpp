#pragma once

#include "rtk/core/Image.hpp"

namespace rtk::core {

struct RenderParams {
  Float4 color_multiplier = {1.0f, 1.0f, 1.0f, 1.0f};
  float source_opacity = 1.0f;
};

struct ColorControlSpec {
  const char* key = "";
  const char* label = "";
  Float4 default_value = {1.0f, 1.0f, 1.0f, 1.0f};
  float display_min = 0.0f;
  float display_max = 1.0f;
};

struct FloatControlSpec {
  const char* key = "";
  const char* label = "";
  float default_value = 0.0f;
  float display_min = 0.0f;
  float display_max = 1.0f;
};

inline constexpr ColorControlSpec kColorMultiplierControl{
    "colorMultiplier",
    "Color Multiplier",
    {1.0f, 1.0f, 1.0f, 1.0f},
    0.0f,
    2.0f,
};

inline constexpr FloatControlSpec kAlphaMultiplierControl{
    "alphaMultiplier",
    "Alpha Multiplier",
    1.0f,
    0.0f,
    1.0f,
};

inline constexpr RenderParams default_render_params() noexcept {
  RenderParams params{};
  params.color_multiplier = kColorMultiplierControl.default_value;
  params.color_multiplier.a = kAlphaMultiplierControl.default_value;
  return params;
}

}  // namespace rtk::core
