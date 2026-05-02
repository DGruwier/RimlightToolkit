#include "rtk/core/Renderer.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

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

bool dimensions_match(const ImageView& source, const MutableImageView& destination) noexcept {
  return source.width == destination.width && source.height == destination.height;
}

std::array<std::uint8_t, 256> make_u8_table(float multiplier) noexcept {
  std::array<std::uint8_t, 256> table{};
  const float scale = std::max(0.0f, multiplier);
  for (int i = 0; i < static_cast<int>(table.size()); ++i) {
    table[static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(std::lround(std::clamp(i * scale, 0.0f, 255.0f)));
  }
  return table;
}

void render_u8(const ImageView& source, const MutableImageView& destination, const RenderParams& params) noexcept {
  const auto red = make_u8_table(params.color_multiplier.r);
  const auto green = make_u8_table(params.color_multiplier.g);
  const auto blue = make_u8_table(params.color_multiplier.b);
  const auto alpha = make_u8_table(clamp01(params.color_multiplier.a) * clamp01(params.source_opacity));

  for (int y = 0; y < source.height; ++y) {
    const auto* src_row = static_cast<const std::uint8_t*>(source.data) + source.row_bytes * y;
    auto* dst_row = static_cast<std::uint8_t*>(destination.data) + destination.row_bytes * y;
    for (int x = 0; x < source.width; ++x) {
      const int offset = x * 4;
      dst_row[offset + 0] = red[src_row[offset + 0]];
      dst_row[offset + 1] = green[src_row[offset + 1]];
      dst_row[offset + 2] = blue[src_row[offset + 2]];
      dst_row[offset + 3] = alpha[src_row[offset + 3]];
    }
  }
}

void render_u16(const ImageView& source, const MutableImageView& destination, const RenderParams& params) noexcept {
  const float red = std::max(0.0f, params.color_multiplier.r) * (1.0f / 65535.0f);
  const float green = std::max(0.0f, params.color_multiplier.g) * (1.0f / 65535.0f);
  const float blue = std::max(0.0f, params.color_multiplier.b) * (1.0f / 65535.0f);
  const float alpha = clamp01(params.color_multiplier.a) * clamp01(params.source_opacity) * (1.0f / 65535.0f);

  for (int y = 0; y < source.height; ++y) {
    const auto* src_row = reinterpret_cast<const std::uint16_t*>(
        static_cast<const std::uint8_t*>(source.data) + source.row_bytes * y);
    auto* dst_row = reinterpret_cast<std::uint16_t*>(
        static_cast<std::uint8_t*>(destination.data) + destination.row_bytes * y);
    for (int x = 0; x < source.width; ++x) {
      const int offset = x * 4;
      dst_row[offset + 0] = to_u16(src_row[offset + 0] * red);
      dst_row[offset + 1] = to_u16(src_row[offset + 1] * green);
      dst_row[offset + 2] = to_u16(src_row[offset + 2] * blue);
      dst_row[offset + 3] = to_u16(src_row[offset + 3] * alpha);
    }
  }
}

void render_f32(const ImageView& source, const MutableImageView& destination, const RenderParams& params) noexcept {
  const float red = std::max(0.0f, params.color_multiplier.r);
  const float green = std::max(0.0f, params.color_multiplier.g);
  const float blue = std::max(0.0f, params.color_multiplier.b);
  const float alpha = clamp01(params.color_multiplier.a) * clamp01(params.source_opacity);

  for (int y = 0; y < source.height; ++y) {
    const auto* src_row = reinterpret_cast<const float*>(
        static_cast<const std::uint8_t*>(source.data) + source.row_bytes * y);
    auto* dst_row = reinterpret_cast<float*>(
        static_cast<std::uint8_t*>(destination.data) + destination.row_bytes * y);
    for (int x = 0; x < source.width; ++x) {
      const int offset = x * 4;
      dst_row[offset + 0] = src_row[offset + 0] * red;
      dst_row[offset + 1] = src_row[offset + 1] * green;
      dst_row[offset + 2] = src_row[offset + 2] * blue;
      dst_row[offset + 3] = src_row[offset + 3] * alpha;
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

  if (source.format == PixelFormat::RgbaU8 && destination.format == PixelFormat::RgbaU8) {
    render_u8(source, destination, params);
    return {};
  }
  if (source.format == PixelFormat::RgbaU16 && destination.format == PixelFormat::RgbaU16) {
    render_u16(source, destination, params);
    return {};
  }
  if (source.format == PixelFormat::RgbaF32 && destination.format == PixelFormat::RgbaF32) {
    render_f32(source, destination, params);
    return {};
  }

  const Float4 multiplier = {
      std::max(0.0f, params.color_multiplier.r),
      std::max(0.0f, params.color_multiplier.g),
      std::max(0.0f, params.color_multiplier.b),
      clamp01(params.color_multiplier.a) * clamp01(params.source_opacity),
  };

  for (int y = 0; y < source.height; ++y) {
    for (int x = 0; x < source.width; ++x) {
      Float4 src = load_pixel(source, x, y);
      store_pixel(destination, x, y, {
          src.r * multiplier.r,
          src.g * multiplier.g,
          src.b * multiplier.b,
          src.a * multiplier.a,
      });
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
