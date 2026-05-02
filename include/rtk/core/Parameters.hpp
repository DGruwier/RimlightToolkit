#pragma once

#include "rtk/core/Image.hpp"

namespace rtk::core {

struct RenderParams {
  float transform_origin_x = -1.0f;
  float transform_origin_y = -1.0f;
  float alpha_scale = 1.08f;
  Float4 fill_color = {1.0f, 1.0f, 1.0f, 1.0f};
  float fill_opacity = 0.75f;
  float source_opacity = 1.0f;
};

}  // namespace rtk::core
