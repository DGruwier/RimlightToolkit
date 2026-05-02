#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
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
#include <vector>

namespace {

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
  rtk::core::RenderParams params;
  RectF image_rect;
  bool dragging = false;
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

PreviewState g_state;
AppOptions g_options;

std::string narrow(const std::wstring& value) {
  if (value.empty()) {
    return {};
  }
  const int size = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()),
                                       nullptr, 0, nullptr, nullptr);
  std::string result(static_cast<std::size_t>(size), '\0');
  WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), result.data(), size,
                      nullptr, nullptr);
  return result;
}

void fill_source() {
  const float cx = g_state.width * 0.45f;
  const float cy = g_state.height * 0.45f;
  const float rx = g_state.width * 0.24f;
  const float ry = g_state.height * 0.30f;

  g_state.source.assign(static_cast<std::size_t>(g_state.width) * g_state.height * 4, 0);
  for (int y = 0; y < g_state.height; ++y) {
    for (int x = 0; x < g_state.width; ++x) {
      const float nx = (x - cx) / rx;
      const float ny = (y - cy) / ry;
      const bool inside = nx * nx + ny * ny <= 1.0f;
      const std::size_t index = (static_cast<std::size_t>(y) * g_state.width + x) * 4;
      g_state.source[index + 0] = 41;
      g_state.source[index + 1] = 184;
      g_state.source[index + 2] = 242;
      g_state.source[index + 3] = inside ? 255 : 0;
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
  g_state.params.transform_origin_x = static_cast<float>(g_state.width - 1) * 0.5f;
  g_state.params.transform_origin_y = static_cast<float>(g_state.height - 1) * 0.5f;
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
      const float bg = checker ? 0.20f : 0.32f;
      const float a = g_state.rendered[index + 3] / 255.0f;
      const float r = (g_state.rendered[index + 0] / 255.0f) * a + bg * (1.0f - a);
      const float g = (g_state.rendered[index + 1] / 255.0f) * a + bg * (1.0f - a);
      const float b = (g_state.rendered[index + 2] / 255.0f) * a + bg * (1.0f - a);
      g_state.display_bgra[index + 0] = static_cast<std::uint8_t>(std::clamp(b, 0.0f, 1.0f) * 255.0f);
      g_state.display_bgra[index + 1] = static_cast<std::uint8_t>(std::clamp(g, 0.0f, 1.0f) * 255.0f);
      g_state.display_bgra[index + 2] = static_cast<std::uint8_t>(std::clamp(r, 0.0f, 1.0f) * 255.0f);
      g_state.display_bgra[index + 3] = 255;
    }
  }
  const auto finished = std::chrono::steady_clock::now();
  return std::chrono::duration<double, std::milli>(finished - started).count();
}

void update_title(HWND hwnd) {
  const std::wstring file = g_state.source_path.empty() ? L"Synthetic Source" : g_state.source_path.filename().wstring();
  wchar_t title[512] = {};
  swprintf_s(title, L"Rimlight Toolkit Preview - %s - scale %.2f opacity %.2f",
             file.c_str(), g_state.params.alpha_scale, g_state.params.fill_opacity);
  SetWindowTextW(hwnd, title);
}

void fit_image_rect(HWND hwnd) {
  RECT client{};
  GetClientRect(hwnd, &client);
  const float client_w = static_cast<float>(std::max(1L, client.right - client.left));
  const float client_h = static_cast<float>(std::max(1L, client.bottom - client.top));
  const float scale = std::min(client_w / static_cast<float>(g_state.width),
                               client_h / static_cast<float>(g_state.height));
  g_state.image_rect.w = static_cast<float>(g_state.width) * scale;
  g_state.image_rect.h = static_cast<float>(g_state.height) * scale;
  g_state.image_rect.x = (client_w - g_state.image_rect.w) * 0.5f;
  g_state.image_rect.y = (client_h - g_state.image_rect.h) * 0.5f;
}

bool client_to_image(int client_x, int client_y, float& image_x, float& image_y) {
  if (client_x < g_state.image_rect.x || client_y < g_state.image_rect.y ||
      client_x > g_state.image_rect.x + g_state.image_rect.w ||
      client_y > g_state.image_rect.y + g_state.image_rect.h) {
    return false;
  }
  image_x = (static_cast<float>(client_x) - g_state.image_rect.x) *
            static_cast<float>(g_state.width) / g_state.image_rect.w;
  image_y = (static_cast<float>(client_y) - g_state.image_rect.y) *
            static_cast<float>(g_state.height) / g_state.image_rect.h;
  return true;
}

void repaint_now(HWND hwnd) {
  InvalidateRect(hwnd, nullptr, FALSE);
  UpdateWindow(hwnd);
}

void set_origin_from_mouse(HWND hwnd, LPARAM lparam) {
  float image_x = 0.0f;
  float image_y = 0.0f;
  if (!client_to_image(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam), image_x, image_y)) {
    return;
  }
  g_state.params.transform_origin_x = image_x;
  g_state.params.transform_origin_y = image_y;
  render_preview();
  repaint_now(hwnd);
}

double draw_preview(HWND hwnd, HDC hdc) {
  const auto started = std::chrono::steady_clock::now();
  fit_image_rect(hwnd);
  RECT client{};
  GetClientRect(hwnd, &client);
  HBRUSH background = CreateSolidBrush(RGB(24, 24, 24));
  FillRect(hdc, &client, background);
  DeleteObject(background);

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

  const int origin_x = static_cast<int>(g_state.image_rect.x +
      (g_state.params.transform_origin_x / static_cast<float>(g_state.width)) * g_state.image_rect.w);
  const int origin_y = static_cast<int>(g_state.image_rect.y +
      (g_state.params.transform_origin_y / static_cast<float>(g_state.height)) * g_state.image_rect.h);

  HPEN pen = CreatePen(PS_SOLID, 1, RGB(255, 210, 64));
  HGDIOBJ old_pen = SelectObject(hdc, pen);
  MoveToEx(hdc, origin_x - 8, origin_y, nullptr);
  LineTo(hdc, origin_x + 9, origin_y);
  MoveToEx(hdc, origin_x, origin_y - 8, nullptr);
  LineTo(hdc, origin_x, origin_y + 9);
  SelectObject(hdc, old_pen);
  DeleteObject(pen);
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

LRESULT CALLBACK window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
  switch (message) {
    case WM_CREATE:
      DragAcceptFiles(hwnd, TRUE);
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
    case WM_LBUTTONDOWN:
      g_state.dragging = true;
      SetCapture(hwnd);
      set_origin_from_mouse(hwnd, lparam);
      return 0;
    case WM_MOUSEMOVE:
      if (g_state.dragging) {
        set_origin_from_mouse(hwnd, lparam);
      }
      return 0;
    case WM_LBUTTONUP:
      g_state.dragging = false;
      ReleaseCapture();
      return 0;
    case WM_KEYDOWN:
      if (wparam == VK_OEM_PLUS || wparam == VK_ADD) {
        g_state.params.alpha_scale = std::min(g_state.params.alpha_scale + 0.02f, 4.0f);
      } else if (wparam == VK_OEM_MINUS || wparam == VK_SUBTRACT) {
        g_state.params.alpha_scale = std::max(g_state.params.alpha_scale - 0.02f, 0.1f);
      } else if (wparam == VK_OEM_6) {
        g_state.params.fill_opacity = std::min(g_state.params.fill_opacity + 0.05f, 1.0f);
      } else if (wparam == VK_OEM_4) {
        g_state.params.fill_opacity = std::max(g_state.params.fill_opacity - 0.05f, 0.0f);
      } else if (wparam == 'S') {
        save_rendered(hwnd);
        return 0;
      } else {
        return 0;
      }
      render_preview();
      update_title(hwnd);
      repaint_now(hwnd);
      return 0;
    case WM_SIZE:
      InvalidateRect(hwnd, nullptr, FALSE);
      return 0;
    case WM_PAINT: {
      PAINTSTRUCT ps{};
      HDC hdc = BeginPaint(hwnd, &ps);
      draw_preview(hwnd, hdc);
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

  fit_image_rect(hwnd);
  for (int i = 0; i < g_options.benchmark_frames; ++i) {
    const auto frame_started = std::chrono::steady_clock::now();
    const float t = static_cast<float>(i) / static_cast<float>(std::max(1, g_options.benchmark_frames - 1));
    g_state.params.transform_origin_x = (0.08f + 0.84f * t) * static_cast<float>(g_state.width - 1);
    g_state.params.transform_origin_y =
        (0.5f + 0.35f * std::sin(t * 6.2831853f)) * static_cast<float>(g_state.height - 1);

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

}  // namespace

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show_command) {
  g_options = parse_options();
  g_state.params.transform_origin_x = static_cast<float>(g_state.width - 1) * 0.5f;
  g_state.params.transform_origin_y = static_cast<float>(g_state.height - 1) * 0.5f;
  fill_source();

  if (!g_options.input.empty()) {
    load_png(g_options.input);
  }
  render_preview();

  const wchar_t class_name[] = L"RimlightToolkitPreviewWindow";
  WNDCLASSW wc{};
  wc.lpfnWndProc = window_proc;
  wc.hInstance = instance;
  wc.hCursor = LoadCursor(nullptr, IDC_CROSS);
  wc.lpszClassName = class_name;
  wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
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
