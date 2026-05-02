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

void u8_color_multiply() {
  std::vector<std::uint8_t> source{100, 120, 140, 200};
  std::vector<std::uint8_t> destination(4, 0);

  rtk::core::RenderParams params;
  params.color_multiplier = {0.5f, 1.0f, 0.25f, 0.5f};

  const rtk::core::ImageView src{source.data(), 1, 1, 4, rtk::core::PixelFormat::RgbaU8};
  const rtk::core::MutableImageView dst{destination.data(), 1, 1, 4, rtk::core::PixelFormat::RgbaU8};

  const auto result = rtk::core::render(src, dst, params);
  assert(result.status == rtk::core::RenderStatus::Ok);
  assert(destination[0] == 50);
  assert(destination[1] == 120);
  assert(destination[2] == 35);
  assert(destination[3] == 100);
}

void f32_color_multiply() {
  std::vector<float> source{0.2f, 0.4f, 0.6f, 1.0f};
  std::vector<float> destination(4, 0.0f);

  rtk::core::RenderParams params;
  params.color_multiplier = {2.0f, 0.5f, 0.25f, 1.0f};

  const rtk::core::ImageView src{source.data(), 1, 1, 4 * static_cast<int>(sizeof(float)), rtk::core::PixelFormat::RgbaF32};
  const rtk::core::MutableImageView dst{destination.data(), 1, 1, 4 * static_cast<int>(sizeof(float)), rtk::core::PixelFormat::RgbaF32};

  const auto result = rtk::core::render(src, dst, params);
  assert(result.status == rtk::core::RenderStatus::Ok);
  assert(std::fabs(destination[0] - 0.4f) < 0.001f);
  assert(std::fabs(destination[1] - 0.2f) < 0.001f);
  assert(std::fabs(destination[2] - 0.15f) < 0.001f);
  assert(std::fabs(destination[3] - 1.0f) < 0.001f);
}

}  // namespace

int main() {
  render_rejects_bad_inputs();
  u8_color_multiply();
  f32_color_multiply();
  return 0;
}
