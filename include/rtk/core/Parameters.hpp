#pragma once

#include "rtk/core/Image.hpp"

namespace rtk::core {

struct RenderParams {
  Float4 color_multiplier = {1.0f, 1.0f, 1.0f, 1.0f};
  float source_opacity = 1.0f;
};

}  // namespace rtk::core
