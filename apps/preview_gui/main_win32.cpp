#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shellapi.h>

#include "lodepng.h"
#include "rtk/core/Renderer.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <numeric>
#include <string>
#include <system_error>
#include <vector>

namespace {

constexpr int kPanelWidth = 310;
constexpr int kPanelPadding = 14;
constexpr int kLabelHeight = 16;
constexpr int kControlHeight = 24;
constexpr int kSliderHeight = 24;
constexpr int kIdMode = 1001;
constexpr int kIdDebug = 1002;
constexpr int kIdOffsetX = 1003;
constexpr int kIdOffsetY = 1004;
constexpr int kIdPointScale = 1005;
constexpr int kIdOcclusion = 1006;
constexpr int kIdBlur = 1007;
constexpr int kIdIterations = 1008;
constexpr int kIdOpacity = 1009;
constexpr int kIdRed = 1010;
constexpr int kIdGreen = 1011;
constexpr int kIdBlue = 1012;
constexpr int kIdEnableAlpha = 1013;
constexpr int kIdEnableOffset = 1014;
constexpr int kIdEnableOcclusion = 1015;
constexpr int kIdEnableBlur = 1016;
constexpr int kIdEnableInvert = 1017;
constexpr int kIdEnableMatte = 1018;
constexpr int kIdEnableColor = 1019;
constexpr int kIdEnableComposite = 1020;

struct RectF {
  float x = 0.0f;
  float y = 0.0f;
  float w = 0.0f;
  float h = 0.0f;
};

struct PreviewState {
  std::vector<std::uint8_t> source;
  std::vector<std::uint8_t> rendered;
  std::vector<std::uint8_t> display_bgra;
  int width = 320;
  int height = 220;
  rtk::core::RenderParams params = rtk::core::default_render_params();
  RectF image_rect;
  std::filesystem::path source_path;
  bool dragging = false;
};

struct AppOptions {
  bool benchmark = false;
  int benchmark_frames = 600;
  std::filesystem::path benchmark_out = "out/benchmark.txt";
  std::filesystem::path input;
};

struct TimingStats {
  std::vector<double> frame_ms;
  std::vector<double> render_ms;
  std::vector<double> draw_ms;
};

struct UiControls {
  HWND mode = nullptr;
  HWND debug = nullptr;
  HWND offset_x = nullptr;
  HWND offset_y = nullptr;
  HWND point_scale = nullptr;
  HWND occlusion = nullptr;
  HWND blur = nullptr;
  HWND iterations = nullptr;
  HWND opacity = nullptr;
  HWND red = nullptr;
  HWND green = nullptr;
  HWND blue = nullptr;
  HWND enable_alpha = nullptr;
  HWND enable_offset = nullptr;
  HWND enable_occlusion = nullptr;
  HWND enable_blur = nullptr;
  HWND enable_invert = nullptr;
  HWND enable_matte = nullptr;
  HWND enable_color = nullptr;
  HWND enable_composite = nullptr;
};

PreviewState g_state;
AppOptions g_options;
UiControls g_ui;

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

std::uint8_t over_checker(std::uint8_t source, std::uint8_t alpha, std::uint8_t background) {
  const int inverse_alpha = 255 - alpha;
  return static_cast<std::uint8_t>((source * alpha + background * inverse_alpha + 127) / 255);
}

int slider_pos(HWND slider) {
  return static_cast<int>(SendMessageW(slider, TBM_GETPOS, 0, 0));
}

bool checked(HWND button) {
  return SendMessageW(button, BM_GETCHECK, 0, 0) == BST_CHECKED;
}

void set_checked(HWND button, bool value) {
  SendMessageW(button, BM_SETCHECK, value ? BST_CHECKED : BST_UNCHECKED, 0);
}

void set_slider(HWND slider, int min_value, int max_value, int value) {
  SendMessageW(slider, TBM_SETRANGE, TRUE, MAKELPARAM(min_value, max_value));
  SendMessageW(slider, TBM_SETPOS, TRUE, std::clamp(value, min_value, max_value));
}

void add_combo_item(HWND combo, const wchar_t* text) {
  SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text));
}

HWND create_label(HWND parent, const wchar_t* text) {
  return CreateWindowExW(0,
                         L"STATIC",
                         text,
                         WS_CHILD | WS_VISIBLE,
                         0,
                         0,
                         10,
                         kLabelHeight,
                         parent,
                         nullptr,
                         GetModuleHandleW(nullptr),
                         nullptr);
}

HWND create_slider(HWND parent, int id) {
  return CreateWindowExW(0,
                         TRACKBAR_CLASSW,
                         L"",
                         WS_CHILD | WS_VISIBLE | TBS_HORZ,
                         0,
                         0,
                         10,
                         kSliderHeight,
                         parent,
                         reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                         GetModuleHandleW(nullptr),
                         nullptr);
}

HWND create_combo(HWND parent, int id) {
  return CreateWindowExW(0,
                         WC_COMBOBOXW,
                         L"",
                         WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
                         0,
                         0,
                         10,
                         120,
                         parent,
                         reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                         GetModuleHandleW(nullptr),
                         nullptr);
}

HWND create_checkbox(HWND parent, int id, const wchar_t* text) {
  return CreateWindowExW(0,
                         L"BUTTON",
                         text,
                         WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                         0,
                         0,
                         10,
                         kControlHeight,
                         parent,
                         reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                         GetModuleHandleW(nullptr),
                         nullptr);
}

void sync_controls_from_params() {
  if (!g_ui.mode) {
    return;
  }

  SendMessageW(g_ui.mode, CB_SETCURSEL, g_state.params.light_mode == rtk::core::LightMode::Directional ? 0 : 1, 0);
  SendMessageW(g_ui.debug, CB_SETCURSEL, static_cast<int>(g_state.params.debug_view), 0);
  set_slider(g_ui.offset_x, -200, 200, static_cast<int>(std::lround(g_state.params.directional_offset_pixels.x)));
  set_slider(g_ui.offset_y, -200, 200, static_cast<int>(std::lround(g_state.params.directional_offset_pixels.y)));
  set_slider(g_ui.point_scale, 50, 300, static_cast<int>(std::lround(g_state.params.point_scale * 100.0f)));
  set_slider(g_ui.occlusion, 0, 160, static_cast<int>(std::lround(g_state.params.occlusion_distance)));
  set_slider(g_ui.blur, 0, 64, static_cast<int>(std::lround(g_state.params.blur_radius)));
  set_slider(g_ui.iterations, 0, 8, g_state.params.blur_iterations);
  set_slider(g_ui.opacity, 0, 100, static_cast<int>(std::lround(g_state.params.solid_opacity * 100.0f)));
  set_slider(g_ui.red, 0, 100, static_cast<int>(std::lround(g_state.params.solid_color.r * 100.0f)));
  set_slider(g_ui.green, 0, 100, static_cast<int>(std::lround(g_state.params.solid_color.g * 100.0f)));
  set_slider(g_ui.blue, 0, 100, static_cast<int>(std::lround(g_state.params.solid_color.b * 100.0f)));

  set_checked(g_ui.enable_alpha, g_state.params.enable.alpha);
  set_checked(g_ui.enable_offset, g_state.params.enable.offset);
  set_checked(g_ui.enable_occlusion, g_state.params.enable.occlusion);
  set_checked(g_ui.enable_blur, g_state.params.enable.fast_blur);
  set_checked(g_ui.enable_invert, g_state.params.enable.invert);
  set_checked(g_ui.enable_matte, g_state.params.enable.matte);
  set_checked(g_ui.enable_color, g_state.params.enable.color_layer);
  set_checked(g_ui.enable_composite, g_state.params.enable.composite);
}

void apply_controls_to_params() {
  if (!g_ui.mode) {
    return;
  }

  g_state.params.light_mode = SendMessageW(g_ui.mode, CB_GETCURSEL, 0, 0) == 0 ? rtk::core::LightMode::Directional
                                                                               : rtk::core::LightMode::Point;
  g_state.params.debug_view = static_cast<rtk::core::DebugView>(
      std::clamp(static_cast<int>(SendMessageW(g_ui.debug, CB_GETCURSEL, 0, 0)), 0, 7));
  g_state.params.directional_offset_pixels.x = static_cast<float>(slider_pos(g_ui.offset_x));
  g_state.params.directional_offset_pixels.y = static_cast<float>(slider_pos(g_ui.offset_y));
  g_state.params.point_scale = static_cast<float>(slider_pos(g_ui.point_scale)) / 100.0f;
  g_state.params.occlusion_distance = static_cast<float>(slider_pos(g_ui.occlusion));
  g_state.params.blur_radius = static_cast<float>(slider_pos(g_ui.blur));
  g_state.params.blur_iterations = slider_pos(g_ui.iterations);
  g_state.params.solid_opacity = static_cast<float>(slider_pos(g_ui.opacity)) / 100.0f;
  g_state.params.solid_color.r = static_cast<float>(slider_pos(g_ui.red)) / 100.0f;
  g_state.params.solid_color.g = static_cast<float>(slider_pos(g_ui.green)) / 100.0f;
  g_state.params.solid_color.b = static_cast<float>(slider_pos(g_ui.blue)) / 100.0f;

  g_state.params.enable.alpha = checked(g_ui.enable_alpha);
  g_state.params.enable.offset = checked(g_ui.enable_offset);
  g_state.params.enable.occlusion = checked(g_ui.enable_occlusion);
  g_state.params.enable.fast_blur = checked(g_ui.enable_blur);
  g_state.params.enable.invert = checked(g_ui.enable_invert);
  g_state.params.enable.matte = checked(g_ui.enable_matte);
  g_state.params.enable.color_layer = checked(g_ui.enable_color);
  g_state.params.enable.composite = checked(g_ui.enable_composite);
}

void create_controls(HWND hwnd) {
  create_label(hwnd, L"Mode");
  g_ui.mode = create_combo(hwnd, kIdMode);
  add_combo_item(g_ui.mode, L"Directional");
  add_combo_item(g_ui.mode, L"Point");

  create_label(hwnd, L"Debug view");
  g_ui.debug = create_combo(hwnd, kIdDebug);
  add_combo_item(g_ui.debug, L"Composite");
  add_combo_item(g_ui.debug, L"Alpha");
  add_combo_item(g_ui.debug, L"Offset");
  add_combo_item(g_ui.debug, L"Occlusion");
  add_combo_item(g_ui.debug, L"Fast Blur");
  add_combo_item(g_ui.debug, L"Invert");
  add_combo_item(g_ui.debug, L"Matte");
  add_combo_item(g_ui.debug, L"Color Layer");

  create_label(hwnd, L"Offset X");
  g_ui.offset_x = create_slider(hwnd, kIdOffsetX);
  create_label(hwnd, L"Offset Y");
  g_ui.offset_y = create_slider(hwnd, kIdOffsetY);
  create_label(hwnd, L"Point scale");
  g_ui.point_scale = create_slider(hwnd, kIdPointScale);
  create_label(hwnd, L"Occlusion distance");
  g_ui.occlusion = create_slider(hwnd, kIdOcclusion);
  create_label(hwnd, L"Blur size");
  g_ui.blur = create_slider(hwnd, kIdBlur);
  create_label(hwnd, L"Blur iterations");
  g_ui.iterations = create_slider(hwnd, kIdIterations);
  create_label(hwnd, L"Opacity");
  g_ui.opacity = create_slider(hwnd, kIdOpacity);
  create_label(hwnd, L"Color red");
  g_ui.red = create_slider(hwnd, kIdRed);
  create_label(hwnd, L"Color green");
  g_ui.green = create_slider(hwnd, kIdGreen);
  create_label(hwnd, L"Color blue");
  g_ui.blue = create_slider(hwnd, kIdBlue);

  g_ui.enable_alpha = create_checkbox(hwnd, kIdEnableAlpha, L"Alpha");
  g_ui.enable_offset = create_checkbox(hwnd, kIdEnableOffset, L"Offset");
  g_ui.enable_occlusion = create_checkbox(hwnd, kIdEnableOcclusion, L"Occlusion");
  g_ui.enable_blur = create_checkbox(hwnd, kIdEnableBlur, L"Fast blur");
  g_ui.enable_invert = create_checkbox(hwnd, kIdEnableInvert, L"Invert");
  g_ui.enable_matte = create_checkbox(hwnd, kIdEnableMatte, L"Matte");
  g_ui.enable_color = create_checkbox(hwnd, kIdEnableColor, L"Color layer");
  g_ui.enable_composite = create_checkbox(hwnd, kIdEnableComposite, L"Composite");

  sync_controls_from_params();
}

void layout_controls(HWND hwnd) {
  RECT client{};
  GetClientRect(hwnd, &client);
  const int panel_x = std::max(0L, client.right - kPanelWidth);
  const int control_x = panel_x + kPanelPadding;
  const int control_w = std::max(40, kPanelWidth - kPanelPadding * 2);
  int y = kPanelPadding;

  HWND child = GetWindow(hwnd, GW_CHILD);
  while (child) {
    wchar_t class_name[32] = {};
    GetClassNameW(child, class_name, 32);
    int height = kControlHeight;
    if (wcscmp(class_name, L"Static") == 0 || wcscmp(class_name, L"STATIC") == 0) {
      height = kLabelHeight;
    } else if (wcscmp(class_name, TRACKBAR_CLASSW) == 0) {
      height = kSliderHeight;
    } else if (wcscmp(class_name, WC_COMBOBOXW) == 0) {
      height = 140;
    }
    MoveWindow(child, control_x, y, control_w, height, TRUE);
    y += (height == 140 ? kControlHeight : height) + 3;
    child = GetWindow(child, GW_HWNDNEXT);
  }
}

void fill_source() {
  g_state.source.assign(static_cast<std::size_t>(g_state.width) * g_state.height * 4, 0);
  for (int y = 0; y < g_state.height; ++y) {
    for (int x = 0; x < g_state.width; ++x) {
      const std::size_t index = (static_cast<std::size_t>(y) * g_state.width + x) * 4;
      g_state.source[index + 0] = static_cast<std::uint8_t>((x * 255) / std::max(1, g_state.width - 1));
      g_state.source[index + 1] = static_cast<std::uint8_t>((y * 255) / std::max(1, g_state.height - 1));
      g_state.source[index + 2] = 180;
      g_state.source[index + 3] = 255;
    }
  }
}

bool load_png(const std::filesystem::path& path) {
  unsigned width = 0;
  unsigned height = 0;
  std::vector<std::uint8_t> pixels;
  const unsigned error = lodepng::decode(pixels, width, height, path.string(), LCT_RGBA, 8);
  if (error != 0) {
    const std::wstring message = L"Could not load PNG:\n" + path.wstring();
    MessageBoxW(nullptr, message.c_str(), L"Rimlight Toolkit", MB_ICONERROR);
    return false;
  }

  g_state.source = std::move(pixels);
  g_state.width = static_cast<int>(width);
  g_state.height = static_cast<int>(height);
  g_state.source_path = path;
  return true;
}

double render_preview() {
  const auto started = std::chrono::steady_clock::now();
  g_state.rendered.resize(g_state.source.size());
  const rtk::core::ImageView source{
      g_state.source.data(), g_state.width, g_state.height, g_state.width * 4, rtk::core::PixelFormat::RgbaU8};
  const rtk::core::MutableImageView destination{
      g_state.rendered.data(), g_state.width, g_state.height, g_state.width * 4, rtk::core::PixelFormat::RgbaU8};
  rtk::core::render(source, destination, g_state.params);

  g_state.display_bgra.resize(g_state.rendered.size());
  for (int y = 0; y < g_state.height; ++y) {
    for (int x = 0; x < g_state.width; ++x) {
      const std::size_t index = (static_cast<std::size_t>(y) * g_state.width + x) * 4;
      const bool checker = ((x / 16) + (y / 16)) % 2 == 0;
      const std::uint8_t background = checker ? 51 : 82;
      const std::uint8_t alpha = g_state.rendered[index + 3];
      g_state.display_bgra[index + 0] = over_checker(g_state.rendered[index + 2], alpha, background);
      g_state.display_bgra[index + 1] = over_checker(g_state.rendered[index + 1], alpha, background);
      g_state.display_bgra[index + 2] = over_checker(g_state.rendered[index + 0], alpha, background);
      g_state.display_bgra[index + 3] = 255;
    }
  }
  const auto finished = std::chrono::steady_clock::now();
  return std::chrono::duration<double, std::milli>(finished - started).count();
}

void update_title(HWND hwnd) {
  const std::wstring file = g_state.source_path.empty() ? L"Synthetic Source" : g_state.source_path.filename().wstring();
  wchar_t title[512] = {};
  swprintf_s(title, L"Rimlight Toolkit Preview - %s - %S %S",
             file.c_str(),
             rtk::core::to_string(g_state.params.light_mode),
             rtk::core::to_string(g_state.params.debug_view));
  SetWindowTextW(hwnd, title);
}

void fit_image_rect(HWND hwnd) {
  RECT client{};
  GetClientRect(hwnd, &client);
  const float client_w = static_cast<float>(std::max(1L, client.right - client.left - kPanelWidth));
  const float client_h = static_cast<float>(std::max(1L, client.bottom - client.top));
  const float scale = std::min(client_w / static_cast<float>(g_state.width),
                               client_h / static_cast<float>(g_state.height));
  g_state.image_rect.w = static_cast<float>(g_state.width) * scale;
  g_state.image_rect.h = static_cast<float>(g_state.height) * scale;
  g_state.image_rect.x = (client_w - g_state.image_rect.w) * 0.5f;
  g_state.image_rect.y = (client_h - g_state.image_rect.h) * 0.5f;
}

rtk::core::Float2 client_to_image(float x, float y) {
  return {
      (x - g_state.image_rect.x) * static_cast<float>(g_state.width) / std::max(1.0f, g_state.image_rect.w),
      (y - g_state.image_rect.y) * static_cast<float>(g_state.height) / std::max(1.0f, g_state.image_rect.h),
  };
}

void update_drag_params(HWND hwnd, LPARAM lparam) {
  fit_image_rect(hwnd);
  const float client_x = static_cast<float>(GET_X_LPARAM(lparam));
  const float client_y = static_cast<float>(GET_Y_LPARAM(lparam));
  const auto image = client_to_image(client_x, client_y);
  if (g_state.params.light_mode == rtk::core::LightMode::Directional) {
    g_state.params.directional_offset_pixels = {
        image.x - static_cast<float>(g_state.width) * 0.5f,
        image.y - static_cast<float>(g_state.height) * 0.5f,
    };
  } else {
    g_state.params.point_source = {
        image.x / static_cast<float>(std::max(1, g_state.width - 1)),
        image.y / static_cast<float>(std::max(1, g_state.height - 1)),
    };
  }
  sync_controls_from_params();
}

void repaint_now(HWND hwnd) {
  InvalidateRect(hwnd, nullptr, FALSE);
  UpdateWindow(hwnd);
}

void draw_indicator(HDC hdc) {
  HPEN pen = CreatePen(PS_SOLID, 2, RGB(255, 220, 80));
  HBRUSH brush = CreateSolidBrush(RGB(255, 220, 80));
  HGDIOBJ old_pen = SelectObject(hdc, pen);
  HGDIOBJ old_brush = SelectObject(hdc, brush);

  if (g_state.params.light_mode == rtk::core::LightMode::Directional) {
    const int cx = static_cast<int>(g_state.image_rect.x + g_state.image_rect.w * 0.5f);
    const int cy = static_cast<int>(g_state.image_rect.y + g_state.image_rect.h * 0.5f);
    const int ex = static_cast<int>(cx + g_state.params.directional_offset_pixels.x * g_state.image_rect.w /
                                             static_cast<float>(std::max(1, g_state.width)));
    const int ey = static_cast<int>(cy + g_state.params.directional_offset_pixels.y * g_state.image_rect.h /
                                             static_cast<float>(std::max(1, g_state.height)));
    Ellipse(hdc, cx - 4, cy - 4, cx + 4, cy + 4);
    MoveToEx(hdc, cx, cy, nullptr);
    LineTo(hdc, ex, ey);
    Ellipse(hdc, ex - 3, ey - 3, ex + 3, ey + 3);
  } else {
    const int px = static_cast<int>(g_state.image_rect.x + g_state.params.point_source.x * g_state.image_rect.w);
    const int py = static_cast<int>(g_state.image_rect.y + g_state.params.point_source.y * g_state.image_rect.h);
    Ellipse(hdc, px - 5, py - 5, px + 5, py + 5);
  }

  SelectObject(hdc, old_brush);
  SelectObject(hdc, old_pen);
  DeleteObject(brush);
  DeleteObject(pen);
}

double draw_preview(HWND hwnd, HDC hdc) {
  const auto started = std::chrono::steady_clock::now();
  fit_image_rect(hwnd);
  RECT client{};
  GetClientRect(hwnd, &client);

  HBRUSH background = CreateSolidBrush(RGB(24, 24, 24));
  FillRect(hdc, &client, background);
  DeleteObject(background);

  RECT panel{std::max(0L, client.right - kPanelWidth), client.top, client.right, client.bottom};
  HBRUSH panel_brush = CreateSolidBrush(RGB(34, 34, 34));
  FillRect(hdc, &panel, panel_brush);
  DeleteObject(panel_brush);

  HPEN separator = CreatePen(PS_SOLID, 1, RGB(58, 58, 58));
  HGDIOBJ old_separator = SelectObject(hdc, separator);
  MoveToEx(hdc, panel.left, panel.top, nullptr);
  LineTo(hdc, panel.left, panel.bottom);
  SelectObject(hdc, old_separator);
  DeleteObject(separator);

  BITMAPINFO info{};
  info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  info.bmiHeader.biWidth = g_state.width;
  info.bmiHeader.biHeight = -g_state.height;
  info.bmiHeader.biPlanes = 1;
  info.bmiHeader.biBitCount = 32;
  info.bmiHeader.biCompression = BI_RGB;

  StretchDIBits(hdc,
                static_cast<int>(g_state.image_rect.x),
                static_cast<int>(g_state.image_rect.y),
                static_cast<int>(g_state.image_rect.w),
                static_cast<int>(g_state.image_rect.h),
                0,
                0,
                g_state.width,
                g_state.height,
                g_state.display_bgra.data(),
                &info,
                DIB_RGB_COLORS,
                SRCCOPY);

  draw_indicator(hdc);
  GdiFlush();
  const auto finished = std::chrono::steady_clock::now();
  return std::chrono::duration<double, std::milli>(finished - started).count();
}

void save_rendered(HWND hwnd) {
  std::filesystem::create_directories("out");
  const std::filesystem::path output = "out/preview-gui.png";
  const unsigned error = lodepng::encode(output.string(), g_state.rendered,
                                         static_cast<unsigned>(g_state.width),
                                         static_cast<unsigned>(g_state.height), LCT_RGBA, 8);
  if (error != 0) {
    MessageBoxW(hwnd, L"Could not save out/preview-gui.png", L"Rimlight Toolkit", MB_ICONERROR);
  } else {
    MessageBoxW(hwnd, L"Saved out/preview-gui.png", L"Rimlight Toolkit", MB_OK);
  }
}

double average(const std::vector<double>& values) {
  if (values.empty()) {
    return 0.0;
  }
  return std::accumulate(values.begin(), values.end(), 0.0) / static_cast<double>(values.size());
}

double percentile(std::vector<double> values, double p) {
  if (values.empty()) {
    return 0.0;
  }
  std::sort(values.begin(), values.end());
  const auto index = static_cast<std::size_t>(std::clamp(p, 0.0, 1.0) * static_cast<double>(values.size() - 1));
  return values[index];
}

void write_benchmark(const TimingStats& stats) {
  const auto parent = g_options.benchmark_out.parent_path();
  if (!parent.empty()) {
    std::filesystem::create_directories(parent);
  }

  const double avg_frame = average(stats.frame_ms);
  const double avg_render = average(stats.render_ms);
  const double avg_draw = average(stats.draw_ms);
  const double fps = avg_frame > 0.0 ? 1000.0 / avg_frame : 0.0;

  std::ofstream out(g_options.benchmark_out);
  out << "frames=" << stats.frame_ms.size() << "\n";
  out << "image_width=" << g_state.width << "\n";
  out << "image_height=" << g_state.height << "\n";
  out << "avg_frame_ms=" << avg_frame << "\n";
  out << "avg_fps=" << fps << "\n";
  out << "avg_render_ms=" << avg_render << "\n";
  out << "avg_draw_ms=" << avg_draw << "\n";
  out << "p95_frame_ms=" << percentile(stats.frame_ms, 0.95) << "\n";
  out << "p95_render_ms=" << percentile(stats.render_ms, 0.95) << "\n";
  out << "p95_draw_ms=" << percentile(stats.draw_ms, 0.95) << "\n";
}

void run_benchmark(HWND hwnd) {
  TimingStats stats;
  stats.frame_ms.reserve(static_cast<std::size_t>(g_options.benchmark_frames));
  stats.render_ms.reserve(static_cast<std::size_t>(g_options.benchmark_frames));
  stats.draw_ms.reserve(static_cast<std::size_t>(g_options.benchmark_frames));

  for (int i = 0; i < g_options.benchmark_frames; ++i) {
    const auto frame_started = std::chrono::steady_clock::now();
    const float t = static_cast<float>(i) / static_cast<float>(std::max(1, g_options.benchmark_frames - 1));
    g_state.params.directional_offset_pixels.x = -40.0f + 80.0f * t;
    g_state.params.directional_offset_pixels.y = -20.0f;

    stats.render_ms.push_back(render_preview());

    HDC hdc = GetDC(hwnd);
    stats.draw_ms.push_back(draw_preview(hwnd, hdc));
    ReleaseDC(hwnd, hdc);

    const auto frame_finished = std::chrono::steady_clock::now();
    stats.frame_ms.push_back(std::chrono::duration<double, std::milli>(frame_finished - frame_started).count());
  }

  write_benchmark(stats);
}

AppOptions parse_options() {
  AppOptions options;
  int argc = 0;
  LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
  for (int i = 1; argv && i < argc; ++i) {
    const std::wstring arg = argv[i];
    if (arg == L"--benchmark") {
      options.benchmark = true;
    } else if (arg == L"--benchmark-frames" && i + 1 < argc) {
      options.benchmark_frames = std::max(1, _wtoi(argv[++i]));
    } else if (arg == L"--benchmark-out" && i + 1 < argc) {
      options.benchmark_out = argv[++i];
    } else if ((arg == L"--input" || arg == L"-i") && i + 1 < argc) {
      options.input = argv[++i];
    } else if (!arg.empty() && arg[0] != L'-') {
      options.input = arg;
    }
  }
  if (argv) {
    LocalFree(argv);
  }
  return options;
}

void update_and_repaint(HWND hwnd) {
  render_preview();
  update_title(hwnd);
  repaint_now(hwnd);
}

LRESULT CALLBACK window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
  switch (message) {
    case WM_CREATE:
      DragAcceptFiles(hwnd, TRUE);
      create_controls(hwnd);
      layout_controls(hwnd);
      return 0;
    case WM_DROPFILES: {
      HDROP drop = reinterpret_cast<HDROP>(wparam);
      wchar_t path[MAX_PATH] = {};
      if (DragQueryFileW(drop, 0, path, MAX_PATH) > 0 && load_png(path)) {
        update_and_repaint(hwnd);
      }
      DragFinish(drop);
      return 0;
    }
    case WM_COMMAND:
      apply_controls_to_params();
      update_and_repaint(hwnd);
      return 0;
    case WM_HSCROLL:
      apply_controls_to_params();
      update_and_repaint(hwnd);
      return 0;
    case WM_LBUTTONDOWN: {
      RECT client{};
      GetClientRect(hwnd, &client);
      if (GET_X_LPARAM(lparam) < std::max(0L, client.right - kPanelWidth)) {
        g_state.dragging = true;
        SetCapture(hwnd);
        update_drag_params(hwnd, lparam);
        update_and_repaint(hwnd);
      }
      return 0;
    }
    case WM_MOUSEMOVE:
      if (g_state.dragging && (wparam & MK_LBUTTON)) {
        update_drag_params(hwnd, lparam);
        update_and_repaint(hwnd);
      }
      return 0;
    case WM_LBUTTONUP:
      if (g_state.dragging) {
        g_state.dragging = false;
        ReleaseCapture();
      }
      return 0;
    case WM_KEYDOWN:
      if (wparam == 'S') {
        save_rendered(hwnd);
      }
      return 0;
    case WM_SIZE:
      layout_controls(hwnd);
      InvalidateRect(hwnd, nullptr, FALSE);
      return 0;
    case WM_ERASEBKGND:
      return 1;
    case WM_PAINT: {
      PAINTSTRUCT ps{};
      HDC hdc = BeginPaint(hwnd, &ps);
      RECT client{};
      GetClientRect(hwnd, &client);
      const int width = std::max(1L, client.right - client.left);
      const int height = std::max(1L, client.bottom - client.top);
      HDC memory_dc = CreateCompatibleDC(hdc);
      HBITMAP memory_bitmap = CreateCompatibleBitmap(hdc, width, height);
      HGDIOBJ old_bitmap = SelectObject(memory_dc, memory_bitmap);
      draw_preview(hwnd, memory_dc);
      BitBlt(hdc, 0, 0, width, height, memory_dc, 0, 0, SRCCOPY);
      SelectObject(memory_dc, old_bitmap);
      DeleteObject(memory_bitmap);
      DeleteDC(memory_dc);
      EndPaint(hwnd, &ps);
      return 0;
    }
    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;
    default:
      return DefWindowProcW(hwnd, message, wparam, lparam);
  }
}

}  // namespace

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show_command) {
  INITCOMMONCONTROLSEX controls{};
  controls.dwSize = sizeof(controls);
  controls.dwICC = ICC_BAR_CLASSES;
  InitCommonControlsEx(&controls);

  g_options = parse_options();
  fill_source();
  if (!g_options.input.empty()) {
    load_png(g_options.input);
  } else if (const auto default_image = default_test_image_path(); std::filesystem::exists(default_image)) {
    load_png(default_image);
  }
  render_preview();

  const wchar_t class_name[] = L"RimlightToolkitPreviewWindow";
  WNDCLASSW wc{};
  wc.lpfnWndProc = window_proc;
  wc.hInstance = instance;
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.lpszClassName = class_name;
  RegisterClassW(&wc);

  HWND hwnd = CreateWindowExW(0,
                              class_name,
                              L"Rimlight Toolkit Preview",
                              WS_OVERLAPPEDWINDOW,
                              CW_USEDEFAULT,
                              CW_USEDEFAULT,
                              1200,
                              860,
                              nullptr,
                              nullptr,
                              instance,
                              nullptr);
  if (!hwnd) {
    return 1;
  }

  update_title(hwnd);
  ShowWindow(hwnd, show_command);
  UpdateWindow(hwnd);

  if (g_options.benchmark) {
    run_benchmark(hwnd);
    DestroyWindow(hwnd);
  }

  MSG msg{};
  while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }
  return static_cast<int>(msg.wParam);
}
