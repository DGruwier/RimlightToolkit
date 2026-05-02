#include "rtk/core/Renderer.hpp"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <vector>

namespace {

void render_rejects_bad_inputs() {
  rtk::core::RenderParams params;
  rtk::core::ImageView source;
  rtk::core::MutableImageView destination;
  const auto result = rtk::core::render(source, destination, params);
  assert(result.status == rtk::core::RenderStatus::InvalidInput);
}

void scaled_inverse_alpha_creates_inner_matte() {
  constexpr int width = 5;
  constexpr int height = 5;
  std::vector<float> source(width * height * 4, 0.0f);
  std::vector<float> destination(width * height * 4, 0.0f);

  const int x = 2;
  const int y = 2;
  const int index = (y * width + x) * 4;
  source[index + 0] = 0.2f;
  source[index + 1] = 0.3f;
  source[index + 2] = 0.4f;
  source[index + 3] = 1.0f;

  rtk::core::RenderParams params;
  params.mode = rtk::core::LightMode::Point;
  params.transform_origin_x = 0.0f;
  params.transform_origin_y = 0.0f;
  params.alpha_scale = 2.0f;
  params.fill_color = {1.0f, 1.0f, 1.0f, 1.0f};
  params.fill_opacity = 0.5f;

  const rtk::core::ImageView src{
      source.data(), width, height, width * 4 * static_cast<int>(sizeof(float)),
      rtk::core::PixelFormat::RgbaF32};
  const rtk::core::MutableImageView dst{
      destination.data(), width, height, width * 4 * static_cast<int>(sizeof(float)),
      rtk::core::PixelFormat::RgbaF32};

  const auto result = rtk::core::render(src, dst, params);
  assert(result.status == rtk::core::RenderStatus::Ok);

  assert(destination[index + 0] > source[index + 0]);
  assert(destination[index + 1] > source[index + 1]);
  assert(destination[index + 2] > source[index + 2]);
  assert(std::fabs(destination[index + 3] - 1.0f) < 0.001f);
}

void directional_offset_creates_inner_matte() {
  constexpr int width = 4;
  constexpr int height = 1;
  std::vector<float> source(width * height * 4, 0.0f);
  std::vector<float> destination(width * height * 4, 0.0f);

  const int x = 1;
  const int index = x * 4;
  source[index + 0] = 0.2f;
  source[index + 1] = 0.3f;
  source[index + 2] = 0.4f;
  source[index + 3] = 1.0f;

  rtk::core::RenderParams params;
  params.mode = rtk::core::LightMode::Directional;
  params.direction_angle_degrees = 0.0f;
  params.direction_distance = 1.0f;
  params.fill_color = {1.0f, 1.0f, 1.0f, 1.0f};
  params.fill_opacity = 0.5f;

  const rtk::core::ImageView src{
      source.data(), width, height, width * 4 * static_cast<int>(sizeof(float)),
      rtk::core::PixelFormat::RgbaF32};
  const rtk::core::MutableImageView dst{
      destination.data(), width, height, width * 4 * static_cast<int>(sizeof(float)),
      rtk::core::PixelFormat::RgbaF32};

  const auto result = rtk::core::render(src, dst, params);
  assert(result.status == rtk::core::RenderStatus::Ok);
  assert(destination[index + 0] > source[index + 0]);
  assert(std::fabs(destination[index + 3] - 1.0f) < 0.001f);
}

void transparent_pixels_do_not_receive_fill() {
  constexpr int width = 2;
  constexpr int height = 1;
  std::vector<std::uint8_t> source(width * height * 4, 0);
  std::vector<std::uint8_t> destination(width * height * 4, 0);
  source[0] = 255;
  source[3] = 255;

  rtk::core::RenderParams params;
  params.mode = rtk::core::LightMode::Point;
  params.transform_origin_x = 0.0f;
  params.transform_origin_y = 0.0f;
  params.alpha_scale = 2.0f;
  params.fill_opacity = 1.0f;

  const rtk::core::ImageView src{source.data(), width, height, width * 4, rtk::core::PixelFormat::RgbaU8};
  const rtk::core::MutableImageView dst{destination.data(), width, height, width * 4, rtk::core::PixelFormat::RgbaU8};

  const auto result = rtk::core::render(src, dst, params);
  assert(result.status == rtk::core::RenderStatus::Ok);
  assert(destination[3] == 255);
  assert(destination[7] == 0);
}

void shadow_view_outputs_cast_mask() {
  constexpr int width = 4;
  constexpr int height = 1;
  std::vector<std::uint8_t> source(width * height * 4, 0);
  std::vector<std::uint8_t> destination(width * height * 4, 0);
  source[3] = 255;

  rtk::core::RenderParams params;
  params.mode = rtk::core::LightMode::Directional;
  params.output_view = rtk::core::OutputView::ShadowMask;
  params.direction_angle_degrees = 0.0f;
  params.shadow_distance = 2.0f;
  params.shadow_strength = 1.0f;

  const rtk::core::ImageView src{source.data(), width, height, width * 4, rtk::core::PixelFormat::RgbaU8};
  const rtk::core::MutableImageView dst{destination.data(), width, height, width * 4, rtk::core::PixelFormat::RgbaU8};

  const auto result = rtk::core::render(src, dst, params);
  assert(result.status == rtk::core::RenderStatus::Ok);
  assert(destination[4] > 0);
  assert(destination[8] > 0);
}

}  // namespace

int main() {
  render_rejects_bad_inputs();
  scaled_inverse_alpha_creates_inner_matte();
  directional_offset_creates_inner_matte();
  transparent_pixels_do_not_receive_fill();
  shadow_view_outputs_cast_mask();
  return 0;
}
