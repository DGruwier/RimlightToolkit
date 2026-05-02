#include "rtk/core/Renderer.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace rtk::core {
namespace {

float clamp01(float value) noexcept {
  return std::clamp(value, 0.0f, 1.0f);
}

std::uint8_t to_u8(float value) noexcept {
  return static_cast<std::uint8_t>(std::lround(clamp01(value) * 255.0f));
}

std::uint16_t to_u16(float value) noexcept {
  return static_cast<std::uint16_t>(std::lround(clamp01(value) * 65535.0f));
}

int rounded_int(float value) noexcept {
  return static_cast<int>(std::lround(value));
}

Float4 load_pixel(const ImageView& image, int x, int y) noexcept {
  const auto* row = static_cast<const std::uint8_t*>(image.data) + image.row_bytes * y;
  switch (image.format) {
    case PixelFormat::RgbaU8: {
      const auto* px = reinterpret_cast<const std::uint8_t*>(row) + x * 4;
      return {px[0] / 255.0f, px[1] / 255.0f, px[2] / 255.0f, px[3] / 255.0f};
    }
    case PixelFormat::RgbaU16: {
      const auto* px = reinterpret_cast<const std::uint16_t*>(row) + x * 4;
      return {px[0] / 65535.0f, px[1] / 65535.0f, px[2] / 65535.0f, px[3] / 65535.0f};
    }
    case PixelFormat::RgbaF32: {
      const auto* px = reinterpret_cast<const float*>(row) + x * 4;
      return {px[0], px[1], px[2], px[3]};
    }
  }
  return {};
}

void store_pixel(const MutableImageView& image, int x, int y, Float4 value) noexcept {
  auto* row = static_cast<std::uint8_t*>(image.data) + image.row_bytes * y;
  switch (image.format) {
    case PixelFormat::RgbaU8: {
      auto* px = reinterpret_cast<std::uint8_t*>(row) + x * 4;
      px[0] = to_u8(value.r);
      px[1] = to_u8(value.g);
      px[2] = to_u8(value.b);
      px[3] = to_u8(value.a);
      return;
    }
    case PixelFormat::RgbaU16: {
      auto* px = reinterpret_cast<std::uint16_t*>(row) + x * 4;
      px[0] = to_u16(value.r);
      px[1] = to_u16(value.g);
      px[2] = to_u16(value.b);
      px[3] = to_u16(value.a);
      return;
    }
    case PixelFormat::RgbaF32: {
      auto* px = reinterpret_cast<float*>(row) + x * 4;
      px[0] = value.r;
      px[1] = value.g;
      px[2] = value.b;
      px[3] = value.a;
      return;
    }
  }
}

Float4 over(Float4 foreground, Float4 background) noexcept {
  foreground.a = clamp01(foreground.a);
  background.a = clamp01(background.a);

  const float out_a = foreground.a + background.a * (1.0f - foreground.a);
  if (out_a <= std::numeric_limits<float>::epsilon()) {
    return {};
  }

  const float fg_weight = foreground.a;
  const float bg_weight = background.a * (1.0f - foreground.a);
  return {
      (foreground.r * fg_weight + background.r * bg_weight) / out_a,
      (foreground.g * fg_weight + background.g * bg_weight) / out_a,
      (foreground.b * fg_weight + background.b * bg_weight) / out_a,
      out_a,
  };
}

void blur_horizontal(const std::vector<float>& input,
                     std::vector<float>& output,
                     int width,
                     int height,
                     int radius) {
  if (radius <= 0) {
    output = input;
    return;
  }

  const int diameter = radius * 2 + 1;
  for (int y = 0; y < height; ++y) {
    float sum = 0.0f;
    for (int ix = -radius; ix <= radius; ++ix) {
      const int x = std::clamp(ix, 0, width - 1);
      sum += input[y * width + x];
    }
    for (int x = 0; x < width; ++x) {
      output[y * width + x] = sum / static_cast<float>(diameter);
      const int remove_x = std::clamp(x - radius, 0, width - 1);
      const int add_x = std::clamp(x + radius + 1, 0, width - 1);
      sum += input[y * width + add_x] - input[y * width + remove_x];
    }
  }
}

void blur_vertical(const std::vector<float>& input,
                   std::vector<float>& output,
                   int width,
                   int height,
                   int radius) {
  if (radius <= 0) {
    output = input;
    return;
  }

  const int diameter = radius * 2 + 1;
  for (int x = 0; x < width; ++x) {
    float sum = 0.0f;
    for (int iy = -radius; iy <= radius; ++iy) {
      const int y = std::clamp(iy, 0, height - 1);
      sum += input[y * width + x];
    }
    for (int y = 0; y < height; ++y) {
      output[y * width + x] = sum / static_cast<float>(diameter);
      const int remove_y = std::clamp(y - radius, 0, height - 1);
      const int add_y = std::clamp(y + radius + 1, 0, height - 1);
      sum += input[add_y * width + x] - input[remove_y * width + x];
    }
  }
}

std::vector<float> make_blurred_offset_alpha(const ImageView& source, const RenderParams& params) {
  const int width = source.width;
  const int height = source.height;
  const int offset_x = rounded_int(params.shadow_offset_x);
  const int offset_y = rounded_int(params.shadow_offset_y);
  const int blur_radius = std::max(0, rounded_int(params.shadow_blur_radius));

  std::vector<float> alpha(width * height, 0.0f);
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const int sx = x - offset_x;
      const int sy = y - offset_y;
      if (sx >= 0 && sx < width && sy >= 0 && sy < height) {
        alpha[y * width + x] = clamp01(load_pixel(source, sx, sy).a);
      }
    }
  }

  if (blur_radius == 0) {
    return alpha;
  }

  std::vector<float> temp(width * height, 0.0f);
  std::vector<float> blurred(width * height, 0.0f);
  blur_horizontal(alpha, temp, width, height, blur_radius);
  blur_vertical(temp, blurred, width, height, blur_radius);
  return blurred;
}

float rim_mask_at(const ImageView& source, int x, int y, int radius) noexcept {
  if (radius <= 0) {
    return 0.0f;
  }

  const float center = clamp01(load_pixel(source, x, y).a);
  float max_neighbor = center;
  for (int dy = -radius; dy <= radius; ++dy) {
    for (int dx = -radius; dx <= radius; ++dx) {
      if (dx * dx + dy * dy > radius * radius) {
        continue;
      }
      const int sx = std::clamp(x + dx, 0, source.width - 1);
      const int sy = std::clamp(y + dy, 0, source.height - 1);
      max_neighbor = std::max(max_neighbor, clamp01(load_pixel(source, sx, sy).a));
    }
  }

  return clamp01(max_neighbor - center);
}

bool dimensions_match(const ImageView& source, const MutableImageView& destination) noexcept {
  return source.width == destination.width && source.height == destination.height;
}

}  // namespace

bool is_valid(const ImageView& image) noexcept {
  return image.data != nullptr && image.width > 0 && image.height > 0 &&
         image.row_bytes >= static_cast<std::ptrdiff_t>(image.width * bytes_per_pixel(image.format));
}

bool is_valid(const MutableImageView& image) noexcept {
  return image.data != nullptr && image.width > 0 && image.height > 0 &&
         image.row_bytes >= static_cast<std::ptrdiff_t>(image.width * bytes_per_pixel(image.format));
}

int bytes_per_pixel(PixelFormat format) noexcept {
  switch (format) {
    case PixelFormat::RgbaU8:
      return 4;
    case PixelFormat::RgbaU16:
      return 8;
    case PixelFormat::RgbaF32:
      return 16;
  }
  return 0;
}

RenderResult render(const ImageView& source,
                    const MutableImageView& destination,
                    const RenderParams& params) {
  if (!is_valid(source) || !is_valid(destination) || !dimensions_match(source, destination)) {
    return {RenderStatus::InvalidInput, "Source and destination must be valid and have matching dimensions."};
  }

  const std::vector<float> shadow_alpha = make_blurred_offset_alpha(source, params);
  const int rim_radius = std::max(0, rounded_int(params.rim_width));
  const float source_opacity = clamp01(params.source_opacity);

  for (int y = 0; y < source.height; ++y) {
    for (int x = 0; x < source.width; ++x) {
      const float shadow_mask = shadow_alpha[y * source.width + x] * clamp01(params.shadow_color.a);
      const Float4 shadow = {
          clamp01(params.shadow_color.r),
          clamp01(params.shadow_color.g),
          clamp01(params.shadow_color.b),
          shadow_mask,
      };

      const float rim_mask = rim_mask_at(source, x, y, rim_radius) *
                             std::max(0.0f, params.rim_intensity) *
                             clamp01(params.rim_color.a);
      const Float4 rim = {
          clamp01(params.rim_color.r),
          clamp01(params.rim_color.g),
          clamp01(params.rim_color.b),
          clamp01(rim_mask),
      };

      Float4 src = load_pixel(source, x, y);
      src.a *= source_opacity;

      const Float4 behind = over(rim, shadow);
      store_pixel(destination, x, y, over(src, behind));
    }
  }

  return {};
}

const char* to_string(RenderStatus status) noexcept {
  switch (status) {
    case RenderStatus::Ok:
      return "Ok";
    case RenderStatus::InvalidInput:
      return "InvalidInput";
    case RenderStatus::UnsupportedFormat:
      return "UnsupportedFormat";
  }
  return "Unknown";
}

}  // namespace rtk::core
