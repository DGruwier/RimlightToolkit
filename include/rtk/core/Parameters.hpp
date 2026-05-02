#pragma once

#include "rtk/core/Image.hpp"

namespace rtk::core {

struct RenderParams {
  float shadow_offset_x = 12.0f;
  float shadow_offset_y = 12.0f;
  float shadow_blur_radius = 10.0f;
  Float4 shadow_color = {0.0f, 0.0f, 0.0f, 0.55f};

  float rim_width = 2.0f;
  float rim_intensity = 0.35f;
  Float4 rim_color = {1.0f, 1.0f, 1.0f, 1.0f};

  float source_opacity = 1.0f;
};

}  // namespace rtk::core
