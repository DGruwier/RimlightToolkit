#pragma once

#include "rtk/core/Image.hpp"

namespace rtk::core {

enum class LightMode {
  Directional,
  Point,
};

struct RenderParams {
  LightMode mode = LightMode::Directional;
  float direction_angle_degrees = 45.0f;
  float direction_distance = 12.0f;

  float transform_origin_x = -1.0f;
  float transform_origin_y = -1.0f;
  float alpha_scale = 1.08f;

  Float4 fill_color = {1.0f, 1.0f, 1.0f, 1.0f};
  float fill_opacity = 0.75f;
  float source_opacity = 1.0f;
};

}  // namespace rtk::core
