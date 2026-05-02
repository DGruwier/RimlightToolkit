#define NOMINMAX
#include <windows.h>
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

constexpr int kPanelWidth = 240;
constexpr int kPanelPadding = 18;
constexpr int kLabelHeight = 20;
constexpr int kSliderHeight = 34;
constexpr int kSliderScale = 100;
constexpr int kIdRed = 1001;
constexpr int kIdGreen = 1002;
constexpr int kIdBlue = 1003;
constexpr int kIdAlpha = 1004;

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
  HWND red = nullptr;
  HWND green = nullptr;
  HWND blue = nullptr;
  HWND alpha = nullptr;
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

int slider_value(float value) {
  return static_cast<int>(std::lround(value * kSliderScale));
}

void set_slider(HWND slider, int value, float min_value, float max_value) {
  SendMessageW(slider,
               TBM_SETRANGE,
               TRUE,
               MAKELPARAM(slider_value(min_value), slider_value(max_value)));
  SendMessageW(slider, TBM_SETPOS, TRUE, value);
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
                         WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_AUTOTICKS,
                         0,
                         0,
                         10,
                         kSliderHeight,
                         parent,
                         reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                         GetModuleHandleW(nullptr),
                         nullptr);
}

void sync_controls_from_params() {
  if (!g_ui.red) {
    return;
  }
  set_slider(g_ui.red,
             slider_value(g_state.params.color_multiplier.r),
             rtk::core::kColorMultiplierControl.display_min,
             rtk::core::kColorMultiplierControl.display_max);
  set_slider(g_ui.green,
             slider_value(g_state.params.color_multiplier.g),
             rtk::core::kColorMultiplierControl.display_min,
             rtk::core::kColorMultiplierControl.display_max);
  set_slider(g_ui.blue,
             slider_value(g_state.params.color_multiplier.b),
             rtk::core::kColorMultiplierControl.display_min,
             rtk::core::kColorMultiplierControl.display_max);
  set_slider(g_ui.alpha,
             slider_value(g_state.params.color_multiplier.a),
             rtk::core::kAlphaMultiplierControl.display_min,
             rtk::core::kAlphaMultiplierControl.display_max);
}

void apply_controls_to_params() {
  if (!g_ui.red) {
    return;
  }
  g_state.params.color_multiplier.r = static_cast<float>(slider_pos(g_ui.red)) / kSliderScale;
  g_state.params.color_multiplier.g = static_cast<float>(slider_pos(g_ui.green)) / kSliderScale;
  g_state.params.color_multiplier.b = static_cast<float>(slider_pos(g_ui.blue)) / kSliderScale;
  g_state.params.color_multiplier.a = static_cast<float>(slider_pos(g_ui.alpha)) / kSliderScale;
}

void create_controls(HWND hwnd) {
  create_label(hwnd, L"Red multiplier");
  g_ui.red = create_slider(hwnd, kIdRed);
  create_label(hwnd, L"Green multiplier");
  g_ui.green = create_slider(hwnd, kIdGreen);
  create_label(hwnd, L"Blue multiplier");
  g_ui.blue = create_slider(hwnd, kIdBlue);
  create_label(hwnd, L"Alpha multiplier");
  g_ui.alpha = create_slider(hwnd, kIdAlpha);
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
    if (wcscmp(class_name, L"Static") == 0 || wcscmp(class_name, L"STATIC") == 0) {
      MoveWindow(child, control_x, y, control_w, kLabelHeight, TRUE);
      y += kLabelHeight;
    } else {
      MoveWindow(child, control_x, y, control_w, kSliderHeight, TRUE);
      y += kSliderHeight + 16;
    }
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
  swprintf_s(title, L"Rimlight Toolkit Preview - %s - RGB %.2f %.2f %.2f Alpha %.2f",
             file.c_str(),
             g_state.params.color_multiplier.r,
             g_state.params.color_multiplier.g,
             g_state.params.color_multiplier.b,
             g_state.params.color_multiplier.a);
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

void repaint_now(HWND hwnd) {
  InvalidateRect(hwnd, nullptr, FALSE);
  UpdateWindow(hwnd);
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
    g_state.params.color_multiplier.r = 0.25f + 1.5f * t;
    g_state.params.color_multiplier.g = 1.75f - 1.5f * t;
    g_state.params.color_multiplier.b = 1.0f;

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
        render_preview();
        update_title(hwnd);
        repaint_now(hwnd);
      }
      DragFinish(drop);
      return 0;
    }
    case WM_HSCROLL:
      apply_controls_to_params();
      render_preview();
      update_title(hwnd);
      repaint_now(hwnd);
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
                              1100,
                              800,
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
