#pragma once

#include <cstddef>
#include <cstdint>

namespace rtk::core {

enum class PixelFormat {
  RgbaU8,
  RgbaU16,
  RgbaF32,
};

struct ImageView {
  const void* data = nullptr;
  int width = 0;
  int height = 0;
  std::ptrdiff_t row_bytes = 0;
  PixelFormat format = PixelFormat::RgbaU8;
};

struct MutableImageView {
  void* data = nullptr;
  int width = 0;
  int height = 0;
  std::ptrdiff_t row_bytes = 0;
  PixelFormat format = PixelFormat::RgbaU8;
};

struct Float4 {
  float r = 0.0f;
  float g = 0.0f;
  float b = 0.0f;
  float a = 0.0f;
};

bool is_valid(const ImageView& image) noexcept;
bool is_valid(const MutableImageView& image) noexcept;
int bytes_per_pixel(PixelFormat format) noexcept;

}  // namespace rtk::core
