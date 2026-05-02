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

void shadow_appears_behind_source() {
  constexpr int width = 8;
  constexpr int height = 8;
  std::vector<float> source(width * height * 4, 0.0f);
  std::vector<float> destination(width * height * 4, 0.0f);

  const int sx = 2;
  const int sy = 2;
  const int source_index = (sy * width + sx) * 4;
  source[source_index + 0] = 1.0f;
  source[source_index + 1] = 0.0f;
  source[source_index + 2] = 0.0f;
  source[source_index + 3] = 1.0f;

  rtk::core::RenderParams params;
  params.shadow_offset_x = 2.0f;
  params.shadow_offset_y = 1.0f;
  params.shadow_blur_radius = 0.0f;
  params.shadow_color = {0.0f, 0.0f, 0.0f, 0.5f};
  params.rim_width = 0.0f;

  const rtk::core::ImageView src{
      source.data(), width, height, width * 4 * static_cast<int>(sizeof(float)),
      rtk::core::PixelFormat::RgbaF32};
  const rtk::core::MutableImageView dst{
      destination.data(), width, height, width * 4 * static_cast<int>(sizeof(float)),
      rtk::core::PixelFormat::RgbaF32};

  const auto result = rtk::core::render(src, dst, params);
  assert(result.status == rtk::core::RenderStatus::Ok);

  const int shadow_index = ((sy + 1) * width + (sx + 2)) * 4;
  assert(std::fabs(destination[shadow_index + 3] - 0.5f) < 0.001f);
  assert(std::fabs(destination[source_index + 0] - 1.0f) < 0.001f);
  assert(std::fabs(destination[source_index + 3] - 1.0f) < 0.001f);
}

void u8_buffers_are_supported() {
  constexpr int width = 2;
  constexpr int height = 1;
  std::vector<std::uint8_t> source(width * height * 4, 0);
  std::vector<std::uint8_t> destination(width * height * 4, 0);
  source[0] = 255;
  source[3] = 255;

  rtk::core::RenderParams params;
  params.shadow_blur_radius = 0.0f;
  params.shadow_offset_x = 1.0f;
  params.shadow_offset_y = 0.0f;
  params.rim_width = 0.0f;

  const rtk::core::ImageView src{source.data(), width, height, width * 4, rtk::core::PixelFormat::RgbaU8};
  const rtk::core::MutableImageView dst{destination.data(), width, height, width * 4, rtk::core::PixelFormat::RgbaU8};

  const auto result = rtk::core::render(src, dst, params);
  assert(result.status == rtk::core::RenderStatus::Ok);
  assert(destination[3] == 255);
  assert(destination[7] > 0);
}

}  // namespace

int main() {
  render_rejects_bad_inputs();
  shadow_appears_behind_source();
  u8_buffers_are_supported();
  return 0;
}
