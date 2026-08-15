# AGENTS.md

C++23 immediate-mode UI library built on SDL2 (`SDL2`, `SDL2_ttf`, `sdl2-gfx`), `spdlog`, `GTest`. All code lives in namespace `DxvUI`.

## Build

- Toolchain: CMake + Ninja, MinGW bundled with CLion, dependencies via vcpkg (CLion-managed, no `vcpkg.json` manifest).
- Existing configured build dir: `cmake-build-debug/` (CLion default, gitignored via `/cmake-build-*/`).

```powershell
# build (existing cache, also re-runs cmake configure automatically)
cmake --build cmake-build-debug

# fresh configure (paths from CMakeCache.txt; $env:VCPKG_ROOT is set)
cmake -S . -B cmake-build-debug -G Ninja `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" `
  -DVCPKG_TARGET_TRIPLET=x64-mingw-dynamic -DCMAKE_BUILD_TYPE=Debug

# run tests (GTest targets auto-discovered via gtest_discover_tests)
ctest --test-dir cmake-build-debug
# or run the binary directly
./cmake-build-debug/DxvUITests.exe
```

## Release build (for performance work)

`cmake-build-release/` is a second, separate build dir. Configure it once, then
reuse `cmake --build` like the debug dir:

```powershell
cmake -S . -B cmake-build-release -G Ninja `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" `
  -DVCPKG_TARGET_TRIPLET=x64-mingw-dynamic -DCMAKE_BUILD_TYPE=Release
cmake --build cmake-build-release
```

## Performance evaluation

Benchmark: `examples/benchmark.cpp` → `DxvUIBenchmark.exe` (both build dirs).

- **Use the Release build.** Debug numbers are meaningless (a 100–1000x
  slowdown hides real regressions). Debug A/B is only for confirming *direction*.
- Benchmark protocol:
  - close all other apps, power plan **High Performance**;
  - run `DxvUIBenchmark.exe --repeats 3` (each scenario reports
    mean/median/min/p95 over all repeats);
  - vsync is **off** by default in the benchmark so frame-phase timing is not
    quantized to the display rate (`--vsync` opts back in);
  - compare **medians**, not means; treat `>= 10%` on a metric as a regression.
- Automated A/B: `scripts/compare.ps1` runs two builds with `--json`, prints a
  delta table and exits 1 if any metric regressed >= 10%:
  ```powershell
  powershell -ExecutionPolicy Bypass -File scripts/compare.ps1 `
    -Baseline cmake-build-debug\DxvUIBenchmark.exe `
    -New cmake-build-release\DxvUIBenchmark.exe -Repeats 3
  ```
- Scenario filter: `--scenario=frames,scroll,hit,text,clip,micro` (comma-separated
  prefixes). `text` = dynamic labels (uncached rasterization + texture-cache
  growth), `clip` = nested `clipContent`, `micro` = raw primitives
  (incl. uncached `rasterize`). `getTextureCacheCount()` exposes the cache size
  for growth checks — the texture cache is LRU-bounded (default 1024 entries),
  so in the `text`/`micro` scenarios growth plateaus at the cap instead of
  growing linearly in unique (font, text, color) keys.
- JSON: `--json` prints a `---JSON---{...}---JSON---` block at the end (all
  metrics, mean/median/min/max/p95/n), which `compare.ps1` parses.

## Gotchas

- **No source globbing.** Every `.cpp` is explicitly listed in `CMakeLists.txt` `target_sources` (lib `DxvUI`, test exe `DxvUITests`). Adding a source file without editing CMakeLists means it silently won't build.
- **Umbrella header `DxvUI/DxvUI.h` includes all public headers.** Keep it in sync when adding a new public header.
- **SDL entry point.** The example defines `extern "C" int SDL_main(...)` and links `SDL2::SDL2main`; SDL2 redefines `main` on Windows.
- **Font selection.** Styles pick a font by logical family (`.fontFamily = "Sans"`), resolved to a platform font file via `getDefaultFontFamilyPath()` (in `core.h`); custom families go through `ITextEngine::registerFontFamily()`. Direct low-level engine calls use `DxvUI::getDefaultFontPath()` (in `core.h`).
- **MinGW runtime mismatch → `0xC0000139` at startup.** Any exe that imports `libspdlogd.dll` fails to load with `STATUS_ENTRYPOINT_NOT_FOUND` if `libstdc++-6.dll` resolves from a toolchain other than CLion's bundled MinGW (e.g. a scoop `mingw-winlibs-ucrt` whose `libstdc++-6.dll` lacks `__cxa_thread_atexit`). CLion works because it puts its own MinGW bin first in PATH for build/run. From a plain shell, prepend CLion's MinGW bin: `$env:PATH = "D:\CLion <ver>\bin\mingw\bin;" + $env:PATH` before `cmake --build` / `ctest`.

## Roadmap

- **Переход с SDL2 на SDL3** (запланировано, в данный момент не выполняется).
  - Зависимости: `find_package(SDL3)` + `find_package(SDL3_ttf)` (vcpkg-порты `sdl3`, `sdl3-ttf`); убрать `sdl2`/`sdl2-ttf`/`sdl2-gfx` и таргеты `SDL2::SDL2main`/`SDL2::SDL2_gfx`.
  - `sdl2-gfx` на SDL3 не портирован: заменить `aacircleRGBA`/`filledCircleRGBA`/`arcRGBA`/`polygonRGBA`/`filledPolygonRGBA` собственной геометрией через `SDL_RenderGeometry` (как уже сделано для rounded rect).
  - Мотивация: SDL2 в режиме поддержки; float-координаты рендера, D3D12-бэкенд с батчингом, high-DPI (`SDL_SetRenderLogicalPresentation`), SDL3_ttf (HarfBuzz-шейпинг, цветные эмодзи, SDF).
  - Внимание: в SDL3 `SDL_Vertex.color` — это `SDL_FColor` (float 0..1); `SDL_Keymod` — keycode-ы модификаторов, нужен перевод в `DxvUI::KeyModifier`; `SDL_StartTextInput` требует `SDL_Window*`.
  - После миграции переснять бенчмарк-базлайны (`scripts/compare.ps1`): смена рендерера может сместить медианы.

## Conventions

- Formatting: `.clang-format` (ColumnLimit 100, indent 4, `PointerAlignment: Left`, includes sorted/regrouped). Project was bulk-formatted in commit `9846733`; run clang-format on touched files.
- `.clang-tidy` is generated from CLion inspections — do not hand-maintain.
- Headers under `include/DxvUI/`, impls in `src/` mirroring the same subdirs (`widgets/`, `containers/`, `renderers/`, `sources/`, `style/`, `layout/`).
- Layout/arrange logic was recently extracted from `SceneNode` into the container classes — put measure/arrange overrides in containers, not `SceneNode`.
- Commit messages and some comments are in Russian; match that when relevant.
