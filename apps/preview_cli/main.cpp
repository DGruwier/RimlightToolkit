#include "rtk/core/Renderer.hpp"

#include "lodepng.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#include <system_error>
#include <vector>

namespace {

struct Args {
  std::filesystem::path input;
  std::filesystem::path out = "out/preview.png";
  int width = 320;
  int height = 220;
  rtk::core::RenderParams params = rtk::core::default_render_params();
};

std::filesystem::path default_test_image_path() {
  const std::filesystem::path relative = std::filesystem::path("assets") / "test_images" / "test_case_john_01.png";
  std::error_code error;
  std::filesystem::path current = std::filesystem::current_path(error);
  if (error) {
    return relative;
  }

  while (!current.empty()) {
    const std::filesystem::path candidate = current / relative;
    if (std::filesystem::exists(candidate, error)) {
      return candidate;
    }
    const std::filesystem::path parent = current.parent_path();
    if (parent == current) {
      break;
    }
    current = parent;
  }

  return relative;
}

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

bool parse_debug_view(const std::string& value, rtk::core::DebugView& view) {
  if (value == "composite") view = rtk::core::DebugView::Composite;
  else if (value == "alpha") view = rtk::core::DebugView::Alpha;
  else if (value == "offset") view = rtk::core::DebugView::Offset;
  else if (value == "occlusion") view = rtk::core::DebugView::Occlusion;
  else if (value == "fast-blur") view = rtk::core::DebugView::FastBlur;
  else if (value == "invert") view = rtk::core::DebugView::Invert;
  else if (value == "matte") view = rtk::core::DebugView::Matte;
  else if (value == "color-layer") view = rtk::core::DebugView::ColorLayer;
  else return false;
  return true;
}

bool parse_args(int argc, char** argv, Args& args) {
  for (int i = 1; i < argc; ++i) {
    const std::string key = argv[i];
    if ((key == "--input" || key == "-i") && i + 1 < argc) {
      args.input = argv[++i];
    } else if ((key == "--out" || key == "-o") && i + 1 < argc) {
      args.out = argv[++i];
    } else if (key == "--width") {
      if (!read_value(i, argc, argv, args.width)) return false;
    } else if (key == "--height") {
      if (!read_value(i, argc, argv, args.height)) return false;
    } else if (key == "--debug" && i + 1 < argc) {
      if (!parse_debug_view(argv[++i], args.params.debug_view)) return false;
    } else if (key == "--mode" && i + 1 < argc) {
      const std::string mode = argv[++i];
      if (mode == "directional") args.params.light_mode = rtk::core::LightMode::Directional;
      else if (mode == "point") args.params.light_mode = rtk::core::LightMode::Point;
      else return false;
    } else if (key == "--offset-x") {
      if (!read_value(i, argc, argv, args.params.directional_offset_pixels.x)) return false;
    } else if (key == "--offset-y") {
      if (!read_value(i, argc, argv, args.params.directional_offset_pixels.y)) return false;
    } else if (key == "--occlusion-slices") {
      if (!read_value(i, argc, argv, args.params.occlusion_slices)) return false;
    } else {
      return false;
    }
  }
  return args.width > 0 && args.height > 0;
}

void fill_source(std::vector<std::uint8_t>& pixels, int width, int height) {
  pixels.assign(static_cast<std::size_t>(width) * height * 4, 0);
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const std::size_t index = (static_cast<std::size_t>(y) * width + x) * 4;
      pixels[index + 0] = static_cast<std::uint8_t>((x * 255) / std::max(1, width - 1));
      pixels[index + 1] = static_cast<std::uint8_t>((y * 255) / std::max(1, height - 1));
      pixels[index + 2] = 180;
      pixels[index + 3] = 255;
    }
  }
}

bool load_png(const std::filesystem::path& path,
              std::vector<std::uint8_t>& pixels,
              int& width,
              int& height) {
  unsigned decoded_width = 0;
  unsigned decoded_height = 0;
  const unsigned error = lodepng::decode(pixels, decoded_width, decoded_height, path.string(), LCT_RGBA, 8);
  if (error != 0) {
    std::cerr << "Could not load PNG " << path << ": " << lodepng_error_text(error) << "\n";
    return false;
  }
  width = static_cast<int>(decoded_width);
  height = static_cast<int>(decoded_height);
  return true;
}

bool write_png(const std::filesystem::path& path,
               const std::vector<std::uint8_t>& pixels,
               int width,
               int height) {
  const auto parent = path.parent_path();
  if (!parent.empty()) {
    std::filesystem::create_directories(parent);
  }

  const unsigned error = lodepng::encode(path.string(), pixels, static_cast<unsigned>(width),
                                         static_cast<unsigned>(height), LCT_RGBA, 8);
  if (error != 0) {
    std::cerr << "Could not write PNG " << path << ": " << lodepng_error_text(error) << "\n";
    return false;
  }
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  Args args;
  if (!parse_args(argc, argv, args)) {
    std::cerr << "Usage: rtk_preview_cli [--input image.png] [--out image.png] [--debug stage] "
                 "[--mode directional|point] [--offset-x pixels] [--offset-y pixels] "
                 "[--occlusion-slices n]\n";
    return 2;
  }

  std::vector<std::uint8_t> source;
  int width = args.width;
  int height = args.height;
  if (!args.input.empty()) {
    if (!load_png(args.input, source, width, height)) {
      return 1;
    }
  } else if (const auto default_image = default_test_image_path(); std::filesystem::exists(default_image)) {
    if (!load_png(default_image, source, width, height)) {
      return 1;
    }
  } else {
    fill_source(source, width, height);
  }

  std::vector<std::uint8_t> destination(source.size(), 0);
  const rtk::core::ImageView src{source.data(), width, height, width * 4, rtk::core::PixelFormat::RgbaU8};
  const rtk::core::MutableImageView dst{destination.data(), width, height, width * 4, rtk::core::PixelFormat::RgbaU8};

  const auto result = rtk::core::render(src, dst, args.params);
  if (result.status != rtk::core::RenderStatus::Ok) {
    std::cerr << "Render failed: " << rtk::core::to_string(result.status) << " " << result.message << "\n";
    return 1;
  }

  if (!write_png(args.out, destination, width, height)) {
    return 1;
  }

  std::cout << "Wrote " << args.out << "\n";
  return 0;
}
