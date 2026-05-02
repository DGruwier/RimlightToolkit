#include "rtk/core/Renderer.hpp"

#include <cassert>
#include <cstdint>
#include <vector>

namespace {

std::vector<std::uint8_t> render_u8(const std::vector<std::uint8_t>& source,
                                    int width,
                                    int height,
                                    rtk::core::RenderParams params) {
  std::vector<std::uint8_t> destination(source.size(), 0);
  const rtk::core::ImageView src{source.data(), width, height, width * 4, rtk::core::PixelFormat::RgbaU8};
  const rtk::core::MutableImageView dst{destination.data(), width, height, width * 4, rtk::core::PixelFormat::RgbaU8};
  const auto result = rtk::core::render(src, dst, params);
  assert(result.status == rtk::core::RenderStatus::Ok);
  return destination;
}

void render_rejects_bad_inputs() {
  rtk::core::RenderParams params;
  rtk::core::ImageView source;
  rtk::core::MutableImageView destination;
  const auto result = rtk::core::render(source, destination, params);
  assert(result.status == rtk::core::RenderStatus::InvalidInput);
}

void alpha_debug_extracts_source_alpha() {
  const std::vector<std::uint8_t> source{10, 20, 30, 128};
  rtk::core::RenderParams params;
  params.debug_view = rtk::core::DebugView::Alpha;

  const auto output = render_u8(source, 1, 1, params);
  assert(output[0] == 128);
  assert(output[1] == 128);
  assert(output[2] == 128);
  assert(output[3] == 255);
}

void directional_offset_moves_mask_by_pixel_vector() {
  const std::vector<std::uint8_t> source{
      0, 0, 0, 255,
      0, 0, 0, 0,
      0, 0, 0, 0,
  };
  rtk::core::RenderParams params;
  params.debug_view = rtk::core::DebugView::Offset;
  params.directional_offset_pixels = {1.0f, 0.0f};

  const auto output = render_u8(source, 3, 1, params);
  assert(output[0] == 0);
  assert(output[4] == 255);
  assert(output[8] == 0);
}

void point_scale_uses_bilinear_sampling_around_pivot() {
  const std::vector<std::uint8_t> source{
      0, 0, 0, 255,
      0, 0, 0, 0,
      0, 0, 0, 0,
  };
  rtk::core::RenderParams params;
  params.light_mode = rtk::core::LightMode::Point;
  params.debug_view = rtk::core::DebugView::Offset;
  params.point_source = {0.0f, 0.0f};
  params.point_scale = 2.0f;

  const auto output = render_u8(source, 3, 1, params);
  assert(output[0] == 255);
  assert(output[4] == 128);
}

void directional_max_blur_propagates_brightest_sample() {
  const std::vector<std::uint8_t> source{
      0, 0, 0, 255,
      0, 0, 0, 0,
      0, 0, 0, 0,
      0, 0, 0, 0,
  };
  rtk::core::RenderParams params;
  params.debug_view = rtk::core::DebugView::Occlusion;
  params.enable.offset = false;
  params.directional_offset_pixels = {1.0f, 0.0f};
  params.occlusion_distance = 2.0f;

  const auto output = render_u8(source, 4, 1, params);
  assert(output[0] == 255);
  assert(output[4] == 255);
  assert(output[8] == 255);
  assert(output[12] == 0);
}

void iterative_box_blur_blurs_mask() {
  const std::vector<std::uint8_t> source{
      0, 0, 0, 0,
      0, 0, 0, 255,
      0, 0, 0, 0,
  };
  rtk::core::RenderParams params;
  params.debug_view = rtk::core::DebugView::FastBlur;
  params.enable.offset = false;
  params.enable.occlusion = false;
  params.blur_radius = 1.0f;
  params.blur_iterations = 1;

  const auto output = render_u8(source, 3, 1, params);
  assert(output[0] == 85);
  assert(output[4] == 85);
  assert(output[8] == 85);
}

void invert_stage_outputs_one_minus_mask() {
  const std::vector<std::uint8_t> source{0, 0, 0, 64};
  rtk::core::RenderParams params;
  params.debug_view = rtk::core::DebugView::Invert;
  params.enable.offset = false;
  params.enable.occlusion = false;
  params.enable.fast_blur = false;

  const auto output = render_u8(source, 1, 1, params);
  assert(output[0] == 191);
}

void matte_multiplies_by_original_alpha() {
  const std::vector<std::uint8_t> source{0, 0, 0, 128};
  rtk::core::RenderParams params;
  params.debug_view = rtk::core::DebugView::Matte;
  params.enable.offset = false;
  params.enable.occlusion = false;
  params.enable.fast_blur = false;
  params.enable.invert = false;

  const auto output = render_u8(source, 1, 1, params);
  assert(output[0] == 64);
}

void color_layer_uses_matte_as_alpha() {
  const std::vector<std::uint8_t> source{0, 0, 0, 255};
  rtk::core::RenderParams params;
  params.debug_view = rtk::core::DebugView::ColorLayer;
  params.enable.offset = false;
  params.enable.occlusion = false;
  params.enable.fast_blur = false;
  params.enable.invert = false;
  params.solid_color = {1.0f, 0.0f, 0.0f, 1.0f};
  params.solid_opacity = 0.5f;

  const auto output = render_u8(source, 1, 1, params);
  assert(output[0] == 255);
  assert(output[1] == 0);
  assert(output[2] == 0);
  assert(output[3] == 128);
}

void composite_alpha_overs_color_layer_on_source() {
  const std::vector<std::uint8_t> source{0, 0, 255, 255};
  rtk::core::RenderParams params;
  params.enable.offset = false;
  params.enable.occlusion = false;
  params.enable.fast_blur = false;
  params.enable.invert = false;
  params.solid_color = {1.0f, 0.0f, 0.0f, 1.0f};
  params.solid_opacity = 0.5f;

  const auto output = render_u8(source, 1, 1, params);
  assert(output[0] == 128);
  assert(output[1] == 0);
  assert(output[2] == 128);
  assert(output[3] == 255);
}

void composite_keeps_straight_color_when_source_is_transparent() {
  const std::vector<std::uint8_t> source{0, 0, 255, 0};
  rtk::core::RenderParams params;
  params.enable.offset = false;
  params.enable.occlusion = false;
  params.enable.fast_blur = false;
  params.enable.invert = false;
  params.enable.alpha = false;
  params.enable.matte = false;
  params.solid_color = {1.0f, 0.0f, 0.0f, 1.0f};
  params.solid_opacity = 0.5f;

  const auto output = render_u8(source, 1, 1, params);
  assert(output[0] == 255);
  assert(output[1] == 0);
  assert(output[2] == 0);
  assert(output[3] == 128);
}

void stage_bypass_passes_input_through() {
  const std::vector<std::uint8_t> source{
      0, 0, 0, 255,
      0, 0, 0, 0,
      0, 0, 0, 0,
  };
  rtk::core::RenderParams params;
  params.debug_view = rtk::core::DebugView::Offset;
  params.enable.offset = false;
  params.directional_offset_pixels = {1.0f, 0.0f};

  const auto output = render_u8(source, 3, 1, params);
  assert(output[0] == 255);
  assert(output[4] == 0);
}

}  // namespace

int main() {
  render_rejects_bad_inputs();
  alpha_debug_extracts_source_alpha();
  directional_offset_moves_mask_by_pixel_vector();
  point_scale_uses_bilinear_sampling_around_pivot();
  directional_max_blur_propagates_brightest_sample();
  iterative_box_blur_blurs_mask();
  invert_stage_outputs_one_minus_mask();
  matte_multiplies_by_original_alpha();
  color_layer_uses_matte_as_alpha();
  composite_alpha_overs_color_layer_on_source();
  composite_keeps_straight_color_when_source_is_transparent();
  stage_bypass_passes_input_through();
  return 0;
}
