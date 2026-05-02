#pragma once

#include "rtk/core/Image.hpp"

namespace rtk::core {

enum class LightMode {
  Directional,
  Point,
};

enum class DebugView {
  Composite,
  Alpha,
  Offset,
  Occlusion,
  FastBlur,
  Invert,
  Matte,
  ColorLayer,
};

struct Float2 {
  float x = 0.0f;
  float y = 0.0f;
};

struct StageEnables {
  bool alpha = true;
  bool offset = true;
  bool occlusion = true;
  bool fast_blur = true;
  bool invert = true;
  bool matte = true;
  bool color_layer = true;
  bool composite = true;
};

struct RenderParams {
  LightMode light_mode = LightMode::Directional;
  DebugView debug_view = DebugView::Composite;

  Float2 directional_offset_pixels = {24.0f, -16.0f};
  Float2 point_source = {0.35f, 0.35f};
  float point_scale = 1.08f;
  float occlusion_distance = 32.0f;
  float blur_radius = 8.0f;
  int blur_iterations = 3;

  Float4 solid_color = {1.0f, 0.82f, 0.36f, 1.0f};
  float solid_opacity = 0.75f;
  StageEnables enable;
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

inline constexpr ColorControlSpec kSolidColorControl{
    "solidColor",
    "Solid Color",
    {1.0f, 0.82f, 0.36f, 1.0f},
    0.0f,
    1.0f,
};

inline constexpr FloatControlSpec kSolidOpacityControl{
    "solidOpacity",
    "Solid Opacity",
    0.75f,
    0.0f,
    1.0f,
};

inline constexpr RenderParams default_render_params() noexcept {
  return {};
}

const char* to_string(LightMode mode) noexcept;
const char* to_string(DebugView view) noexcept;

}  // namespace rtk::core
