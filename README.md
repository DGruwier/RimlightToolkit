# Rimlight Toolkit

Rimlight Toolkit is a native C++ scaffold for host-independent image-processing logic plus native After Effects and OpenFX adapters. The current processing stack is deliberately reset to a simple color multiplier so the previewer, launch scripts, and plug-in scaffolding can be cleaned up around a stable baseline.

- A host-independent renderer library.
- A fast preview/test harness for visual iteration outside host applications.
- Adobe After Effects as a native effect plug-in.
- OpenFX hosts such as Nuke, Resolve, Fusion, Natron, and Flame.

The current repository is intentionally structured so most algorithm work happens in `rtk_core`. Host adapters only translate host pixels and parameters into the core API.
The core also owns the small parameter schema used by the previewer and adapter scaffolds, so labels/defaults/ranges stay tied to the render contract instead of drifting across hosts.

## Layout

```text
include/rtk/core/          Public host-independent C++ API
src/core/                  CPU renderer implementation
apps/preview_cli/          Small CLI preview harness for parameter iteration
adapters/aftereffects/     Optional AE SmartFX-style adapter scaffold
adapters/ofx/              Optional OpenFX image-effect adapter scaffold
assets/test_images/        Default visual test inputs for preview and smoke testing
tests/                     Core renderer regression tests
cmake/                     SDK discovery helpers
docs/                      Architecture and SDK notes
```

## Build Core, Tests, and Preview Harness

Quick path:

```powershell
.\scripts\rtk.ps1 run
```

On Windows this opens the native interactive preview window using `assets/test_images/test_case_john_01.png` by default. Drag a PNG from Explorer onto the window to replace it. The preview currently exposes only RGBA multiplier sliders. You can also force the window explicitly with `.\scripts\rtk.ps1 gui`.

Run the interactive-preview benchmark:

```powershell
.\scripts\rtk.ps1 bench --benchmark-frames 600
```

The benchmark starts the native preview executable, animates the multiplier controls through the same render/draw path, writes `out\benchmark.txt`, and prints average FPS plus frame/render/draw timing statistics.

Process a PNG:

```powershell
.\scripts\rtk.ps1 run --input .\path\to\source.png --out out\preview.png
```

If no PNG is supplied, the CLI preview uses the same default test image and falls back to a synthetic gradient only when the asset is unavailable.

Process a PNG with the CLI:

```powershell
.\scripts\rtk.ps1 run --input .\source.png --out out\preview.png --mul-r 1 --mul-g 0.75 --mul-b 0.5
```

Passing arguments runs the CLI preview harness instead. On Windows, `scripts\preview.cmd` can be used as a simple Explorer drop target: drag a PNG onto it and it will build/run the interactive preview against that image.

On macOS/Linux:

```bash
./scripts/rtk.sh run
```

The launch scripts support `check`, `build`, `test`, `run`, and `clean`. They verify required dependencies, configure the build if needed, build the latest sources, locate the newest preview harness binary, and run it with any preview arguments passed through. On Windows, `rtk.ps1` will download a repo-local portable CMake into `.rtk/tools` when CMake is not already on `PATH`, then use Ninja or an installed Visual Studio generator.

Manual path:

```powershell
cmake -S . -B build -G "Ninja"
cmake --build build
ctest --test-dir build --output-on-failure
```

Generate a synthetic preview frame:

```powershell
.\build\apps\preview_cli\rtk_preview_cli.exe
```

The preview output is a PNG file so it can be inspected in common image tools.

## Optional SDK Builds

After Effects and OFX targets are disabled by default because production SDKs are external.

```powershell
cmake -S . -B build-ae -DRTK_BUILD_AE_PLUGIN=ON -DAE_SDK_ROOT="C:\path\to\AfterEffectsSDK"
cmake --build build-ae

cmake -S . -B build-ofx -DRTK_BUILD_OFX_PLUGIN=ON -DOPENFX_SDK_ROOT="C:\path\to\OpenFX"
cmake --build build-ofx
```

The adapter code is designed to remain thin. Missing SDK files should only affect the optional adapter target, never the core renderer or preview harness.
The OpenFX adapter is also compiled in CI with fetched OpenFX headers to catch scaffold drift. The After Effects adapter still requires a local Adobe SDK because those headers are not redistributed here.

## Development Direction

The first implementation path is CPU-first, deterministic, and testable:

1. Normalize host input into `rtk::core::ImageView` / `MutableImageView`.
2. Render color multiplication in `rtk::core::render`.
3. Map AE 8/16-bpc CPU worlds and OFX byte/short/float RGBA buffers at adapter boundaries.
4. Add GPU parity later through backend entry points that preserve the same core parameter contract.

See [docs/architecture.md](docs/architecture.md) and [docs/sdk-notes.md](docs/sdk-notes.md).
