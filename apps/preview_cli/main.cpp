#include "rtk/core/Renderer.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

struct Args {
  std::filesystem::path out = "out/preview.ppm";
  int width = 320;
  int height = 220;
  float blur = 10.0f;
  float offset_x = 14.0f;
  float offset_y = 12.0f;
  float rim_width = 2.0f;
  float rim_intensity = 0.35f;
};

bool read_value(int& i, int argc, char** argv, float& target) {
  if (i + 1 >= argc) {
    return false;
  }
  target = std::stof(argv[++i]);
  return true;
}

bool read_value(int& i, int argc, char** argv, int& target) {
  if (i + 1 >= argc) {
    return false;
  }
  target = std::stoi(argv[++i]);
  return true;
}

bool parse_args(int argc, char** argv, Args& args) {
  for (int i = 1; i < argc; ++i) {
    const std::string key = argv[i];
    if (key == "--out" && i + 1 < argc) {
      args.out = argv[++i];
    } else if (key == "--width" && !read_value(i, argc, argv, args.width)) {
      return false;
    } else if (key == "--height" && !read_value(i, argc, argv, args.height)) {
      return false;
    } else if (key == "--blur" && !read_value(i, argc, argv, args.blur)) {
      return false;
    } else if (key == "--offset-x" && !read_value(i, argc, argv, args.offset_x)) {
      return false;
    } else if (key == "--offset-y" && !read_value(i, argc, argv, args.offset_y)) {
      return false;
    } else if (key == "--rim-width" && !read_value(i, argc, argv, args.rim_width)) {
      return false;
    } else if (key == "--rim-intensity" && !read_value(i, argc, argv, args.rim_intensity)) {
      return false;
    } else {
      return false;
    }
  }
  return args.width > 0 && args.height > 0;
}

void fill_source(std::vector<float>& pixels, int width, int height) {
  const float cx = width * 0.45f;
  const float cy = height * 0.45f;
  const float rx = width * 0.24f;
  const float ry = height * 0.30f;

  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const float nx = (x - cx) / rx;
      const float ny = (y - cy) / ry;
      const bool inside = nx * nx + ny * ny <= 1.0f;
      const std::size_t index = (static_cast<std::size_t>(y) * width + x) * 4;
      pixels[index + 0] = 0.16f;
      pixels[index + 1] = 0.72f;
      pixels[index + 2] = 0.95f;
      pixels[index + 3] = inside ? 1.0f : 0.0f;
    }
  }
}

bool write_ppm(const std::filesystem::path& path, const std::vector<float>& pixels, int width, int height) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::binary);
  if (!out) {
    return false;
  }

  out << "P6\n" << width << " " << height << "\n255\n";
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const std::size_t index = (static_cast<std::size_t>(y) * width + x) * 4;
      const auto to_byte = [](float value) {
        value = std::clamp(value, 0.0f, 1.0f);
        return static_cast<unsigned char>(value * 255.0f + 0.5f);
      };
      const unsigned char rgb[3] = {
          to_byte(pixels[index + 0] * pixels[index + 3]),
          to_byte(pixels[index + 1] * pixels[index + 3]),
          to_byte(pixels[index + 2] * pixels[index + 3]),
      };
      out.write(reinterpret_cast<const char*>(rgb), 3);
    }
  }
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  Args args;
  if (!parse_args(argc, argv, args)) {
    std::cerr << "Usage: rtk_preview_cli [--out path] [--width n] [--height n] "
                 "[--blur px] [--offset-x px] [--offset-y px] [--rim-width px] "
                 "[--rim-intensity value]\n";
    return 2;
  }

  std::vector<float> source(static_cast<std::size_t>(args.width) * args.height * 4, 0.0f);
  std::vector<float> destination(source.size(), 0.0f);
  fill_source(source, args.width, args.height);

  rtk::core::RenderParams params;
  params.shadow_blur_radius = args.blur;
  params.shadow_offset_x = args.offset_x;
  params.shadow_offset_y = args.offset_y;
  params.rim_width = args.rim_width;
  params.rim_intensity = args.rim_intensity;

  const rtk::core::ImageView src{
      source.data(), args.width, args.height, args.width * 4 * static_cast<int>(sizeof(float)),
      rtk::core::PixelFormat::RgbaF32};
  const rtk::core::MutableImageView dst{
      destination.data(), args.width, args.height, args.width * 4 * static_cast<int>(sizeof(float)),
      rtk::core::PixelFormat::RgbaF32};

  const auto result = rtk::core::render(src, dst, params);
  if (result.status != rtk::core::RenderStatus::Ok) {
    std::cerr << "Render failed: " << rtk::core::to_string(result.status) << " " << result.message << "\n";
    return 1;
  }

  if (!write_ppm(args.out, destination, args.width, args.height)) {
    std::cerr << "Could not write " << args.out << "\n";
    return 1;
  }

  std::cout << "Wrote " << args.out << "\n";
  return 0;
}
