# Rimlight Toolkit

Rimlight Toolkit is a native C++ scaffold for a 2D composited shadow and rimlight effect targeting:

- A host-independent renderer library.
- A fast preview/test harness for visual iteration outside host applications.
- Adobe After Effects as a native effect plug-in.
- OpenFX hosts such as Nuke, Resolve, Fusion, Natron, and Flame.

The current repository is intentionally structured so most algorithm work happens in `rtk_core`. Host adapters only translate host pixels and parameters into the core API.

## Layout

```text
include/rtk/core/          Public host-independent C++ API
src/core/                  CPU renderer implementation
apps/preview_cli/          Small CLI preview harness for parameter iteration
adapters/aftereffects/     Optional AE SmartFX-style adapter scaffold
adapters/ofx/              Optional OpenFX image-effect adapter scaffold
tests/                     Core renderer regression tests
cmake/                     SDK discovery helpers
docs/                      Architecture and SDK notes
```

## Build Core, Tests, and Preview Harness

Quick path:

```powershell
.\scripts\rtk.ps1 run
```

On Windows this opens the native interactive preview window. Drag a PNG from Explorer onto the window to load it. Directional mode is the default and uses angle/distance controls for a uniform alpha offset. In directional mode, click-drag around the image center to set the angle directly on the canvas. Enable `Point source` to use the position/scale path; in that mode, click or drag in the canvas to move the transform origin. The `View` dropdown can inspect the final output, base mask, shadow mask, or blurred mask. You can also force the window explicitly with `.\scripts\rtk.ps1 gui`.

Run the interactive-preview benchmark:

```powershell
.\scripts\rtk.ps1 bench --benchmark-frames 600
```

The benchmark starts the native preview executable, simulates a drag path through the same render/draw path, writes `out\benchmark.txt`, and prints average FPS plus frame/render/draw timing statistics.

Process a PNG:

```powershell
.\scripts\rtk.ps1 run --input .\path\to\source.png --out out\preview.png
```

The CLI supports both render paths:

```powershell
.\scripts\rtk.ps1 run --input .\source.png --mode directional --angle 45 --distance 16
.\scripts\rtk.ps1 run --input .\source.png --mode point --scale 1.12 --origin-x 160 --origin-y 100
.\scripts\rtk.ps1 run --input .\source.png --view shadow
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

## Development Direction

The first implementation path is CPU-first, deterministic, and testable:

1. Normalize host input into `rtk::core::ImageView` / `MutableImageView`.
2. Render shadow, rim, and source composite in `rtk::core::render`.
3. Map AE 8/16/32-bpc and OFX byte/short/float RGBA buffers at adapter boundaries.
4. Add GPU parity later through backend entry points that preserve the same core parameter contract.

See [docs/architecture.md](docs/architecture.md) and [docs/sdk-notes.md](docs/sdk-notes.md).
