#include "rtk/core/Renderer.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace rtk::core {
namespace {

constexpr float kPi = 3.14159265358979323846f;

float clamp01(float value) noexcept {
  return std::clamp(value, 0.0f, 1.0f);
}

std::uint8_t to_u8(float value) noexcept {
  return static_cast<std::uint8_t>(std::lround(clamp01(value) * 255.0f));
}

std::uint16_t to_u16(float value) noexcept {
  return static_cast<std::uint16_t>(std::lround(clamp01(value) * 65535.0f));
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

float alpha_at(const ImageView& image, int x, int y) noexcept {
  if (x < 0 || y < 0 || x >= image.width || y >= image.height) {
    return 0.0f;
  }
  return clamp01(load_pixel(image, x, y).a);
}

float sample_alpha_bilinear(const ImageView& image, float x, float y) noexcept {
  if (x < 0.0f || y < 0.0f || x > static_cast<float>(image.width - 1) ||
      y > static_cast<float>(image.height - 1)) {
    return 0.0f;
  }

  const int x0 = static_cast<int>(std::floor(x));
  const int y0 = static_cast<int>(std::floor(y));
  const int x1 = std::min(x0 + 1, image.width - 1);
  const int y1 = std::min(y0 + 1, image.height - 1);
  const float tx = x - static_cast<float>(x0);
  const float ty = y - static_cast<float>(y0);

  const float a00 = alpha_at(image, x0, y0);
  const float a10 = alpha_at(image, x1, y0);
  const float a01 = alpha_at(image, x0, y1);
  const float a11 = alpha_at(image, x1, y1);
  const float ax0 = a00 + (a10 - a00) * tx;
  const float ax1 = a01 + (a11 - a01) * tx;
  return ax0 + (ax1 - ax0) * ty;
}

void transformed_alpha_sample_position(const ImageView& source,
                                       const RenderParams& params,
                                       int x,
                                       int y,
                                       float& sample_x,
                                       float& sample_y) noexcept {
  if (params.mode == LightMode::Directional) {
    const float radians = params.direction_angle_degrees * (kPi / 180.0f);
    sample_x = static_cast<float>(x) - std::cos(radians) * params.direction_distance;
    sample_y = static_cast<float>(y) - std::sin(radians) * params.direction_distance;
    return;
  }

  const float scale = std::max(params.alpha_scale, 0.0001f);
  const float origin_x = params.transform_origin_x >= 0.0f
                             ? params.transform_origin_x
                             : (static_cast<float>(source.width - 1) * 0.5f);
  const float origin_y = params.transform_origin_y >= 0.0f
                             ? params.transform_origin_y
                             : (static_cast<float>(source.height - 1) * 0.5f);

  sample_x = origin_x + (static_cast<float>(x) - origin_x) / scale;
  sample_y = origin_y + (static_cast<float>(y) - origin_y) / scale;
}

float inverse_alpha_matted_with_source(float original_alpha, float transformed_alpha) noexcept {
  return clamp01((1.0f - transformed_alpha) * original_alpha);
}

float transformed_inverse_alpha_mask_at(const ImageView& source,
                                        const RenderParams& params,
                                        int x,
                                        int y) noexcept {
  float sx = 0.0f;
  float sy = 0.0f;
  transformed_alpha_sample_position(source, params, x, y, sx, sy);

  const float original_alpha = alpha_at(source, x, y);
  const float transformed_alpha = sample_alpha_bilinear(source, sx, sy);
  return inverse_alpha_matted_with_source(original_alpha, transformed_alpha);
}

bool dimensions_match(const ImageView& source, const MutableImageView& destination) noexcept {
  return source.width == destination.width && source.height == destination.height;
}

float alpha_at_u8(const ImageView& image, int x, int y) noexcept {
  if (x < 0 || y < 0 || x >= image.width || y >= image.height) {
    return 0.0f;
  }
  const auto* row = static_cast<const std::uint8_t*>(image.data) + image.row_bytes * y;
  return (row[x * 4 + 3] * (1.0f / 255.0f));
}

float sample_alpha_bilinear_u8(const ImageView& image, float x, float y) noexcept {
  if (x < 0.0f || y < 0.0f || x > static_cast<float>(image.width - 1) ||
      y > static_cast<float>(image.height - 1)) {
    return 0.0f;
  }

  const int x0 = static_cast<int>(x);
  const int y0 = static_cast<int>(y);
  const int x1 = std::min(x0 + 1, image.width - 1);
  const int y1 = std::min(y0 + 1, image.height - 1);
  const float tx = x - static_cast<float>(x0);
  const float ty = y - static_cast<float>(y0);

  const float a00 = alpha_at_u8(image, x0, y0);
  const float a10 = alpha_at_u8(image, x1, y0);
  const float a01 = alpha_at_u8(image, x0, y1);
  const float a11 = alpha_at_u8(image, x1, y1);
  const float ax0 = a00 + (a10 - a00) * tx;
  const float ax1 = a01 + (a11 - a01) * tx;
  return ax0 + (ax1 - ax0) * ty;
}

void render_u8(const ImageView& source, const MutableImageView& destination, const RenderParams& params) {
  const float scale = std::max(params.alpha_scale, 0.0001f);
  const float inverse_scale = 1.0f / scale;
  const float direction_radians = params.direction_angle_degrees * (kPi / 180.0f);
  const float direction_x = std::cos(direction_radians) * params.direction_distance;
  const float direction_y = std::sin(direction_radians) * params.direction_distance;
  const float origin_x = params.transform_origin_x >= 0.0f
                             ? params.transform_origin_x
                             : (static_cast<float>(source.width - 1) * 0.5f);
  const float origin_y = params.transform_origin_y >= 0.0f
                             ? params.transform_origin_y
                             : (static_cast<float>(source.height - 1) * 0.5f);
  const float fill_opacity = clamp01(params.fill_opacity) * clamp01(params.fill_color.a);
  const float source_opacity = clamp01(params.source_opacity);
  const float fill_r = clamp01(params.fill_color.r);
  const float fill_g = clamp01(params.fill_color.g);
  const float fill_b = clamp01(params.fill_color.b);

  for (int y = 0; y < source.height; ++y) {
    const auto* src_row = static_cast<const std::uint8_t*>(source.data) + source.row_bytes * y;
    auto* dst_row = static_cast<std::uint8_t*>(destination.data) + destination.row_bytes * y;
    const float point_sy = origin_y + (static_cast<float>(y) - origin_y) * inverse_scale;
    const float directional_sy = static_cast<float>(y) - direction_y;

    for (int x = 0; x < source.width; ++x) {
      const int offset = x * 4;
      const float src_a = (src_row[offset + 3] * (1.0f / 255.0f)) * source_opacity;
      const float point_sx = origin_x + (static_cast<float>(x) - origin_x) * inverse_scale;
      const float directional_sx = static_cast<float>(x) - direction_x;
      const float sample_x = params.mode == LightMode::Directional ? directional_sx : point_sx;
      const float sample_y = params.mode == LightMode::Directional ? directional_sy : point_sy;
      const float sampled_alpha = sample_alpha_bilinear_u8(source, sample_x, sample_y);
      const float fill_a = inverse_alpha_matted_with_source(src_a, sampled_alpha) * fill_opacity;
      const float out_a = fill_a + src_a * (1.0f - fill_a);

      if (out_a <= std::numeric_limits<float>::epsilon()) {
        dst_row[offset + 0] = 0;
        dst_row[offset + 1] = 0;
        dst_row[offset + 2] = 0;
        dst_row[offset + 3] = 0;
        continue;
      }

      const float src_weight = src_a * (1.0f - fill_a);
      const float inv_out_a = 1.0f / out_a;
      dst_row[offset + 0] = to_u8((fill_r * fill_a + (src_row[offset + 0] * (1.0f / 255.0f)) * src_weight) * inv_out_a);
      dst_row[offset + 1] = to_u8((fill_g * fill_a + (src_row[offset + 1] * (1.0f / 255.0f)) * src_weight) * inv_out_a);
      dst_row[offset + 2] = to_u8((fill_b * fill_a + (src_row[offset + 2] * (1.0f / 255.0f)) * src_weight) * inv_out_a);
      dst_row[offset + 3] = to_u8(out_a);
    }
  }
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

  const float fill_opacity = clamp01(params.fill_opacity) * clamp01(params.fill_color.a);
  const float source_opacity = clamp01(params.source_opacity);

  if (source.format == PixelFormat::RgbaU8 && destination.format == PixelFormat::RgbaU8) {
    render_u8(source, destination, params);
    return {};
  }

  for (int y = 0; y < source.height; ++y) {
    for (int x = 0; x < source.width; ++x) {
      Float4 src = load_pixel(source, x, y);
      src.a *= source_opacity;

      const float mask = transformed_inverse_alpha_mask_at(source, params, x, y);
      const Float4 fill = {
          clamp01(params.fill_color.r),
          clamp01(params.fill_color.g),
          clamp01(params.fill_color.b),
          clamp01(mask * fill_opacity),
      };

      store_pixel(destination, x, y, over(fill, src));
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
