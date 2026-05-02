#include "rtk/core/Renderer.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numeric>
#include <utility>
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

bool dimensions_match(const ImageView& source, const MutableImageView& destination) noexcept {
  return source.width == destination.width && source.height == destination.height;
}

std::size_t mask_index(int x, int y, int width) noexcept {
  return static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x);
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

float sample_mask(const std::vector<float>& mask, int width, int height, float x, float y) noexcept {
  if (x < 0.0f || y < 0.0f || x > static_cast<float>(width - 1) || y > static_cast<float>(height - 1)) {
    return 0.0f;
  }

  const int x0 = static_cast<int>(std::floor(x));
  const int y0 = static_cast<int>(std::floor(y));
  const int x1 = std::min(width - 1, x0 + 1);
  const int y1 = std::min(height - 1, y0 + 1);
  const float tx = x - static_cast<float>(x0);
  const float ty = y - static_cast<float>(y0);
  const float a = mask[mask_index(x0, y0, width)] * (1.0f - tx) + mask[mask_index(x1, y0, width)] * tx;
  const float b = mask[mask_index(x0, y1, width)] * (1.0f - tx) + mask[mask_index(x1, y1, width)] * tx;
  return a * (1.0f - ty) + b * ty;
}

void copy_mask(const std::vector<float>& source, std::vector<float>& destination) {
  destination = source;
}

void extract_alpha(const ImageView& source, std::vector<float>& alpha) {
  alpha.assign(static_cast<std::size_t>(source.width) * static_cast<std::size_t>(source.height), 0.0f);
  for (int y = 0; y < source.height; ++y) {
    for (int x = 0; x < source.width; ++x) {
      alpha[mask_index(x, y, source.width)] = clamp01(load_pixel(source, x, y).a);
    }
  }
}

void offset_directional(const std::vector<float>& input,
                        std::vector<float>& output,
                        int width,
                        int height,
                        Float2 offset) {
  output.assign(input.size(), 0.0f);
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      output[mask_index(x, y, width)] = sample_mask(input, width, height, x - offset.x, y - offset.y);
    }
  }
}

void offset_point(const std::vector<float>& input,
                  std::vector<float>& output,
                  int width,
                  int height,
                  Float2 point_source,
                  float scale) {
  output.assign(input.size(), 0.0f);
  const float pivot_x = point_source.x * static_cast<float>(std::max(1, width - 1));
  const float pivot_y = point_source.y * static_cast<float>(std::max(1, height - 1));
  const float safe_scale = std::max(0.01f, scale);
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const float sample_x = pivot_x + (static_cast<float>(x) - pivot_x) / safe_scale;
      const float sample_y = pivot_y + (static_cast<float>(y) - pivot_y) / safe_scale;
      output[mask_index(x, y, width)] = sample_mask(input, width, height, sample_x, sample_y);
    }
  }
}

void max_blur_directional(const std::vector<float>& input,
                          std::vector<float>& output,
                          int width,
                          int height,
                          Float2 vector,
                          float distance) {
  output.assign(input.size(), 0.0f);
  float length = std::sqrt(vector.x * vector.x + vector.y * vector.y);
  if (length < 0.001f) {
    vector = {1.0f, 0.0f};
    length = 1.0f;
  }
  int step_x = static_cast<int>(std::lround(vector.x));
  int step_y = static_cast<int>(std::lround(vector.y));
  if (step_x == 0 && step_y == 0) {
    step_x = static_cast<int>(std::lround(vector.x / length));
    step_y = static_cast<int>(std::lround(vector.y / length));
  }
  if (step_x == 0 && step_y == 0) {
    step_x = 1;
  }
  const int divisor = std::gcd(std::abs(step_x), std::abs(step_y));
  if (divisor > 1) {
    step_x /= divisor;
    step_y /= divisor;
  }

  const float step_length = std::max(1.0f, std::sqrt(static_cast<float>(step_x * step_x + step_y * step_y)));
  const int steps = std::max(0, static_cast<int>(std::ceil(std::max(0.0f, distance))));
  const int sample_count = static_cast<int>(std::ceil(static_cast<float>(steps) / step_length));
  std::vector<std::pair<int, int>> offsets;
  offsets.reserve(static_cast<std::size_t>(sample_count + 1));
  for (int i = 0; i <= sample_count; ++i) {
    offsets.push_back({step_x * i, step_y * i});
  }

  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      float value = 0.0f;
      for (const auto& offset : offsets) {
        const int sx = x - offset.first;
        const int sy = y - offset.second;
        if (sx >= 0 && sy >= 0 && sx < width && sy < height) {
          value = std::max(value, input[mask_index(sx, sy, width)]);
        }
      }
      output[mask_index(x, y, width)] = value;
    }
  }
}

void max_blur_point(const std::vector<float>& input,
                    std::vector<float>& output,
                    int width,
                    int height,
                    Float2 point_source,
                    float distance) {
  output.assign(input.size(), 0.0f);
  const float pivot_x = point_source.x * static_cast<float>(std::max(1, width - 1));
  const float pivot_y = point_source.y * static_cast<float>(std::max(1, height - 1));
  const int steps = std::max(0, static_cast<int>(std::ceil(std::max(0.0f, distance))));
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      float dx = static_cast<float>(x) - pivot_x;
      float dy = static_cast<float>(y) - pivot_y;
      const float length = std::sqrt(dx * dx + dy * dy);
      if (length < 0.001f) {
        dx = 1.0f;
        dy = 0.0f;
      } else {
        dx /= length;
        dy /= length;
      }
      float value = input[mask_index(x, y, width)];
      for (int i = 1; i <= steps; ++i) {
        value = std::max(value, sample_mask(input, width, height, x - dx * i, y - dy * i));
      }
      output[mask_index(x, y, width)] = value;
    }
  }
}

void box_blur_once(const std::vector<float>& input,
                   std::vector<float>& output,
                   std::vector<float>& scratch,
                   int width,
                   int height,
                   int radius) {
  if (radius <= 0) {
    copy_mask(input, output);
    return;
  }

  scratch.assign(input.size(), 0.0f);
  output.assign(input.size(), 0.0f);

  for (int y = 0; y < height; ++y) {
    float sum = 0.0f;
    for (int x = -radius; x <= radius; ++x) {
      const int sx = std::clamp(x, 0, width - 1);
      sum += input[mask_index(sx, y, width)];
    }
    for (int x = 0; x < width; ++x) {
      scratch[mask_index(x, y, width)] = sum / static_cast<float>(radius * 2 + 1);
      const int remove_x = std::clamp(x - radius, 0, width - 1);
      const int add_x = std::clamp(x + radius + 1, 0, width - 1);
      sum += input[mask_index(add_x, y, width)] - input[mask_index(remove_x, y, width)];
    }
  }

  for (int x = 0; x < width; ++x) {
    float sum = 0.0f;
    for (int y = -radius; y <= radius; ++y) {
      const int sy = std::clamp(y, 0, height - 1);
      sum += scratch[mask_index(x, sy, width)];
    }
    for (int y = 0; y < height; ++y) {
      output[mask_index(x, y, width)] = sum / static_cast<float>(radius * 2 + 1);
      const int remove_y = std::clamp(y - radius, 0, height - 1);
      const int add_y = std::clamp(y + radius + 1, 0, height - 1);
      sum += scratch[mask_index(x, add_y, width)] - scratch[mask_index(x, remove_y, width)];
    }
  }
}

void iterative_box_blur(const std::vector<float>& input,
                        std::vector<float>& output,
                        std::vector<float>& scratch,
                        int width,
                        int height,
                        int radius,
                        int iterations) {
  if (radius <= 0 || iterations <= 0) {
    copy_mask(input, output);
    return;
  }

  std::vector<float> current = input;
  std::vector<float> blurred;
  for (int i = 0; i < iterations; ++i) {
    box_blur_once(current, blurred, scratch, width, height, radius);
    current.swap(blurred);
  }
  output = std::move(current);
}

void invert_mask(const std::vector<float>& input, std::vector<float>& output) {
  output.resize(input.size());
  for (std::size_t i = 0; i < input.size(); ++i) {
    output[i] = 1.0f - clamp01(input[i]);
  }
}

void multiply_masks(const std::vector<float>& a, const std::vector<float>& b, std::vector<float>& output) {
  output.resize(a.size());
  for (std::size_t i = 0; i < a.size(); ++i) {
    output[i] = clamp01(a[i]) * clamp01(b[i]);
  }
}

Float4 color_layer_pixel(const RenderParams& params, float alpha) noexcept {
  const float a = clamp01(alpha) * clamp01(params.solid_opacity) * clamp01(params.solid_color.a);
  return {
      clamp01(params.solid_color.r),
      clamp01(params.solid_color.g),
      clamp01(params.solid_color.b),
      a,
  };
}

Float4 over(Float4 top, Float4 bottom) noexcept {
  const float inverse = 1.0f - clamp01(top.a);
  const float alpha = top.a + bottom.a * inverse;
  if (alpha <= 0.0f) {
    return {};
  }
  return {
      (top.r * top.a + bottom.r * bottom.a * inverse) / alpha,
      (top.g * top.a + bottom.g * bottom.a * inverse) / alpha,
      (top.b * top.a + bottom.b * bottom.a * inverse) / alpha,
      alpha,
  };
}

void write_mask_debug(const std::vector<float>& mask, const MutableImageView& destination) {
  for (int y = 0; y < destination.height; ++y) {
    for (int x = 0; x < destination.width; ++x) {
      const float value = clamp01(mask[mask_index(x, y, destination.width)]);
      store_pixel(destination, x, y, {value, value, value, 1.0f});
    }
  }
}

void write_color_layer(const std::vector<float>& mask, const MutableImageView& destination, const RenderParams& params) {
  for (int y = 0; y < destination.height; ++y) {
    for (int x = 0; x < destination.width; ++x) {
      const Float4 color = params.enable.color_layer ? color_layer_pixel(params, mask[mask_index(x, y, destination.width)])
                                                     : Float4{};
      store_pixel(destination, x, y, color);
    }
  }
}

void write_composite(const ImageView& source,
                     const std::vector<float>& mask,
                     const MutableImageView& destination,
                     const RenderParams& params) {
  for (int y = 0; y < destination.height; ++y) {
    for (int x = 0; x < destination.width; ++x) {
      const Float4 src = load_pixel(source, x, y);
      if (!params.enable.composite) {
        store_pixel(destination, x, y, src);
        continue;
      }
      store_pixel(destination, x, y, over(color_layer_pixel(params, mask[mask_index(x, y, destination.width)]), src));
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

  std::vector<float> alpha;
  std::vector<float> stage;
  std::vector<float> next;
  std::vector<float> scratch;

  extract_alpha(source, alpha);
  const std::vector<float> original_alpha = alpha;
  if (!params.enable.alpha) {
    std::fill(alpha.begin(), alpha.end(), 1.0f);
  }
  copy_mask(alpha, stage);
  if (params.debug_view == DebugView::Alpha) {
    write_mask_debug(stage, destination);
    return {};
  }

  if (params.enable.offset) {
    if (params.light_mode == LightMode::Directional) {
      offset_directional(stage, next, source.width, source.height, params.directional_offset_pixels);
    } else {
      offset_point(stage, next, source.width, source.height, params.point_source, params.point_scale);
    }
    stage.swap(next);
  }
  if (params.debug_view == DebugView::Offset) {
    write_mask_debug(stage, destination);
    return {};
  }

  if (params.enable.occlusion) {
    if (params.light_mode == LightMode::Directional) {
      max_blur_directional(stage, next, source.width, source.height, params.directional_offset_pixels, params.occlusion_distance);
    } else {
      max_blur_point(stage, next, source.width, source.height, params.point_source, params.occlusion_distance);
    }
    stage.swap(next);
  }
  if (params.debug_view == DebugView::Occlusion) {
    write_mask_debug(stage, destination);
    return {};
  }

  if (params.enable.fast_blur) {
    iterative_box_blur(stage,
                       next,
                       scratch,
                       source.width,
                       source.height,
                       static_cast<int>(std::lround(std::max(0.0f, params.blur_radius))),
                       std::max(0, params.blur_iterations));
    stage.swap(next);
  }
  if (params.debug_view == DebugView::FastBlur) {
    write_mask_debug(stage, destination);
    return {};
  }

  if (params.enable.invert) {
    invert_mask(stage, next);
    stage.swap(next);
  }
  if (params.debug_view == DebugView::Invert) {
    write_mask_debug(stage, destination);
    return {};
  }

  if (params.enable.matte) {
    multiply_masks(stage, original_alpha, next);
    stage.swap(next);
  }
  if (params.debug_view == DebugView::Matte) {
    write_mask_debug(stage, destination);
    return {};
  }

  if (params.debug_view == DebugView::ColorLayer) {
    write_color_layer(stage, destination, params);
    return {};
  }

  if (!params.enable.color_layer) {
    for (int y = 0; y < destination.height; ++y) {
      for (int x = 0; x < destination.width; ++x) {
        store_pixel(destination, x, y, load_pixel(source, x, y));
      }
    }
    return {};
  }

  write_composite(source, stage, destination, params);
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

const char* to_string(LightMode mode) noexcept {
  switch (mode) {
    case LightMode::Directional:
      return "Directional";
    case LightMode::Point:
      return "Point";
  }
  return "Unknown";
}

const char* to_string(DebugView view) noexcept {
  switch (view) {
    case DebugView::Composite:
      return "Composite";
    case DebugView::Alpha:
      return "Alpha";
    case DebugView::Offset:
      return "Offset";
    case DebugView::Occlusion:
      return "Occlusion";
    case DebugView::FastBlur:
      return "FastBlur";
    case DebugView::Invert:
      return "Invert";
    case DebugView::Matte:
      return "Matte";
    case DebugView::ColorLayer:
      return "ColorLayer";
  }
  return "Unknown";
}

}  // namespace rtk::core
