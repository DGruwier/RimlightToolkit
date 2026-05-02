#pragma once

#include "rtk/core/Image.hpp"

namespace rtk::core {

enum class LightMode {
  Directional,
  Point,
};

enum class OutputView {
  Final,
  BaseMask,
  ShadowMask,
  BlurredMask,
};

struct RenderParams {
  LightMode mode = LightMode::Directional;
  OutputView output_view = OutputView::Final;
  float direction_angle_degrees = 45.0f;
  float direction_distance = 12.0f;

  float transform_origin_x = -1.0f;
  float transform_origin_y = -1.0f;
  float alpha_scale = 1.08f;

  float mask_blur_radius = 1.0f;
  int mask_blur_iterations = 1;
  float shadow_distance = 4.0f;
  float shadow_strength = 0.65f;

  Float4 fill_color = {1.0f, 1.0f, 1.0f, 1.0f};
  float fill_opacity = 0.75f;
  float source_opacity = 1.0f;
};

}  // namespace rtk::core
