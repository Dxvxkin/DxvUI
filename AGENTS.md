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

## Gotchas

- **No source globbing.** Every `.cpp` is explicitly listed in `CMakeLists.txt` `target_sources` (lib `DxvUI`, test exe `DxvUITests`). Adding a source file without editing CMakeLists means it silently won't build.
- **Umbrella header `DxvUI/DxvUI.h` includes all public headers.** Keep it in sync when adding a new public header.
- **SDL entry point.** The example defines `extern "C" int SDL_main(...)` and links `SDL2::SDL2main`; SDL2 redefines `main` on Windows.
- **Windows-only example code.** `examples/main.cpp` hardcodes `C:/Windows/Fonts/segoeui.ttf`. Use `DxvUI::getDefaultFontPath()` (in `core.h`) for cross-platform paths.
- **MinGW runtime mismatch → `0xC0000139` at startup.** Any exe that imports `libspdlogd.dll` fails to load with `STATUS_ENTRYPOINT_NOT_FOUND` if `libstdc++-6.dll` resolves from a toolchain other than CLion's bundled MinGW (e.g. a scoop `mingw-winlibs-ucrt` whose `libstdc++-6.dll` lacks `__cxa_thread_atexit`). CLion works because it puts its own MinGW bin first in PATH for build/run. From a plain shell, prepend CLion's MinGW bin: `$env:PATH = "D:\CLion <ver>\bin\mingw\bin;" + $env:PATH` before `cmake --build` / `ctest`.

## Conventions

- Formatting: `.clang-format` (ColumnLimit 100, indent 4, `PointerAlignment: Left`, includes sorted/regrouped). Project was bulk-formatted in commit `9846733`; run clang-format on touched files.
- `.clang-tidy` is generated from CLion inspections — do not hand-maintain.
- Headers under `include/DxvUI/`, impls in `src/` mirroring the same subdirs (`widgets/`, `containers/`, `renderers/`, `sources/`, `style/`, `layout/`).
- Layout/arrange logic was recently extracted from `SceneNode` into the container classes — put measure/arrange overrides in containers, not `SceneNode`.
- Commit messages and some comments are in Russian; match that when relevant.
