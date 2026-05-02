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

The preview output is a plain PPM file so it can be inspected in common image tools without pulling a GUI dependency into the initial scaffold.

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
