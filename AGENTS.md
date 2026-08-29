# AGENTS.md

C++23 immediate-mode UI library built on SDL2 (`SDL2`, `SDL2_ttf`, `sdl2-gfx`), `spdlog`, `GTest`. All code lives in
namespace `DxvUI`.

## Build

- Toolchain: CMake + Ninja, MinGW (рекомендуется bundled с CLion), зависимости — **vcpkg manifest mode**: зависимости
  объявлены в `vcpkg.json` (builtin-baseline пиннит релиз) и ставятся автоматически при configure в
  `<builddir>/vcpkg_installed`. Подойдёт любой валидный клон vcpkg — достаточно переменной `VCPKG_ROOT`; если
  baseline-коммит не найден, обновите клон (`git pull` / `git fetch --tags` + checkout тега). Первый configure на новой
  машине собирает порты из исходников (~10–40 мин один раз; дальше локальный binary-cache
  `%LOCALAPPDATA%\vcpkg\archives` / `~/.cache/vcpkg/archives`).
- Пресеты задают `VCPKG_HOST_TRIPLET` = целевому триплету: хостовые порты-помощники (`vcpkg-cmake`,
  `vcpkg-cmake-config`) иначе ставятся под дефолтным host-триплетом (`x64-windows`), который требует Visual Studio — на
  машинах без VS configure падает с «Unable to find a valid Visual Studio instance».
- Конфигурация — через `CMakePresets.json`: `debug`/`release` (Windows,
  `x64-mingw-dynamic`) и `linux-debug`/`linux-release` (Linux, `x64-linux-dynamic`); binaryDir'ы совпадают с
  CLion-овскими (`cmake-build-{debug,release}/`, gitignored). CLion работает как раньше (свои профили, те же каталоги);
  VS Code — расширение CMake Tools (пресеты подхватываются автоматически) + clangd.

```powershell
# Windows: configure + build + tests
cmake --preset debug            # или release; на Linux: linux-debug / linux-release
cmake --build cmake-build-debug
ctest --test-dir cmake-build-debug
# или напрямую (GTest-тесты автодискаверятся через gtest_discover_tests)
./cmake-build-debug/DxvUITests.exe

# пересборка без переустановки портов — просто build по существующему кэшу:
cmake --build cmake-build-release
```

## Работа между Windows и Linux (git-воркфлоу)

Обе машины работают напрямую с единой веткой `master` на GitHub (единственный источник правды); локальных «OS-веток»
нет.

- **Правила**:
    - перед началом работы и перед пушем — обязательно стянуть свежую историю: `git pull --ff-only` (Windows) /
      `git pull --rebase` (Linux);
    - никогда не использовать `git push --force`;
    - конфликты разрешаются локально, правки пушатся отдельными осмысленными коммитами.
- **Windows**: работа на `master` (или в короткоживущей ветке с fast-forward-мержем в master); перед пушем снова
  `git pull --ff-only` — пуш всегда идёт fast-forward.
- **Linux**: работа на отслеживаемом `origin/master`; перед пушем `git pull --rebase` — локальные коммиты накладываются
  поверх свежих чужих, история остаётся линейной и пуш проходит без non-fast-forward.
- Синхронизация `AGENTS.md` и любых правок между машинами происходит сама через пул/пуш; общая документация держится в
  актуальном виде на GitHub.

## Release build (for performance work)

`cmake-build-release/` is a second, separate build dir:

```powershell
cmake --preset release
cmake --build cmake-build-release
```

## Packaging / consumption by third parties

- `cmake --install <builddir> --prefix <dir>` ставит headers (`include/DxvUI/*`), статическую библиотеку и CMake-пакет
  (`lib/cmake/DxvUI`: `DxvUIConfig.cmake`,
  `DxvUIConfigVersion.cmake`, `DxvUITargets.cmake`, namespace `DxvUI::`). Версия пакета берётся из
  `project(... VERSION ...)` (SameMajorVersion).
- Подключение снаружи: `find_package(DxvUI REQUIRED CONFIG)` +
  `target_link_libraries(app PRIVATE DxvUI::DxvUI)`; публичные зависимости (SDL2/SDL2_ttf/sdl2-gfx/spdlog) подтягиваются
  через `find_dependency`.
- Потребитель на vcpkg должен иметь свой `vcpkg.json` с теми же зависимостями (иначе toolchain в classic-режиме не
  найдёт SDL2). Console-app потребителю нужен
  `#define SDL_MAIN_HANDLED` до включения заголовков DxvUI — зонтичный
  `DxvUI/DxvUI.h` тянет `<SDL.h>`, который на Windows редиректит `main` → `SDL_main`.
- Dev-experience в репо: таргет `dxvui_mingw_runtime` копирует MinGW-runtime DLL
  (`libstdc++-6/libgcc_s_seh-1/libwinpthread-1`) рядом с экзешниками в `bin/`
  (vcpkg-овские DLL туда уже кладёт z-applocal), поэтому собранные бинарники запускаются двойным кликом без настройки
  PATH.

## Performance evaluation

Benchmark: `examples/benchmark.cpp` → `DxvUIBenchmark.exe` (both build dirs).

- **Use the Release build.** Debug numbers are meaningless (a 100–1000x slowdown hides real regressions). Debug A/B is
  only for confirming *direction*.
- Benchmark protocol:
    - close all other apps, power plan **High Performance**;
    - run `DxvUIBenchmark.exe --repeats 3` (each scenario reports mean/median/min/p95 over all repeats);
    - vsync is **off** by default in the benchmark so frame-phase timing is not quantized to the display rate (`--vsync`
      opts back in);
    - compare **medians**, not means; treat `>= 10%` on a metric as a regression.
- Automated A/B: `scripts/compare.ps1` runs two builds with `--json`, prints a delta table and exits 1 if any metric
  regressed >= 10%:
  ```powershell
  powershell -ExecutionPolicy Bypass -File scripts/compare.ps1 `
    -Baseline cmake-build-debug\DxvUIBenchmark.exe `
    -New cmake-build-release\DxvUIBenchmark.exe -Repeats 3
  ```
- Scenario filter: `--scenario=frames,scroll,hit,text,clip,micro` (comma-separated prefixes). `text` = dynamic labels
  (uncached rasterization + texture-cache growth), `clip` = nested `clipContent`, `micro` = raw primitives (incl.
  uncached `rasterize`). `getTextureCacheCount()` exposes the cache size for growth checks — the texture cache is
  LRU-bounded (default 1024 entries), so in the `text`/`micro` scenarios growth plateaus at the cap instead of growing
  linearly in unique (font, text, color) keys.
- JSON: `--json` prints a `---JSON---{...}---JSON---` block at the end (all metrics, mean/median/min/max/p95/n), which
  `compare.ps1` parses.

## Gotchas

- **No source globbing.** Every `.cpp` is explicitly listed in `CMakeLists.txt` `target_sources` (lib `DxvUI`, test exe
  `DxvUITests`). Adding a source file without editing CMakeLists means it silently won't build.
- **Umbrella header `DxvUI/DxvUI.h` includes all public headers.** Keep it in sync when adding a new public header.
- **SDL entry point.** The example defines `extern "C" int SDL_main(...)` and links `SDL2::SDL2main`; SDL2 redefines
  `main` on Windows.
- **Font selection.** Styles pick a font by logical family (`.fontFamily = "Sans"`), resolved to a platform font file
  via `getDefaultFontFamilyPath()` (in `core.h`); custom families go through `ITextEngine::registerFontFamily()`. Direct
  low-level engine calls use `DxvUI::getDefaultFontPath()` (in `core.h`).
- **MinGW runtime mismatch → `0xC0000139` at startup.** Any exe that imports `libspdlogd.dll` fails to load with
  `STATUS_ENTRYPOINT_NOT_FOUND` if `libstdc++-6.dll` resolves from a toolchain other than CLion's bundled MinGW (e.g. a
  scoop `mingw-winlibs-ucrt` whose `libstdc++-6.dll` lacks `__cxa_thread_atexit`). CLion works because it puts its own
  MinGW bin first in PATH for build/run. From a plain shell, prepend CLion's MinGW bin:
  `$env:PATH = "D:\CLion <ver>\bin\mingw\bin;" + $env:PATH` before `cmake --build` / `ctest`. То же касается первого
  manifest-install: vcpkg собирает порты найденным на PATH MinGW — держите его первым, чтобы порты и проект были собраны
  одним компилятором (иначе ABI-микс).

## Conventions

- Formatting: `.clang-format` (ColumnLimit 100, indent 4, `PointerAlignment: Left`, includes sorted/regrouped). Project
  was bulk-formatted in commit `9846733`; run clang-format on touched files.
- `.clang-tidy` is generated from CLion inspections — do not hand-maintain.
- Headers under `include/DxvUI/`, impls in `src/` mirroring the same subdirs (`widgets/`, `containers/`, `renderers/`,
  `sources/`, `style/`, `layout/`).
- Layout/arrange logic was recently extracted from `SceneNode` into the container classes — put measure/arrange
  overrides in containers, not `SceneNode`.
- Commit messages and some comments are in Russian; match that when relevant.
