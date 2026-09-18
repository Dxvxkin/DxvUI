# AGENTS.md

C++23 immediate-mode UI library built on SDL2 (`SDL2`, `SDL2_ttf`), `spdlog`, `GTest`. All code lives in namespace
`DxvUI`.

## Build

- Toolchain: CMake + Ninja; компилятор не закреплён в пресетах — CMake берёт доступный (Linux: системный gcc/clang;
  Windows: любой однородный MinGW-тулчейн на PATH, например bundled с CLion или scoop `mingw-winlibs-ucrt`).
  FetchContent-депсы собираются тем же компилятором, что и проект. Зависимости — политика **find-or-fetch**
  (`cmake/deps.cmake`): каждая сначала ищется через `find_package` в системе, а если не найдена — тянется
  `FetchContent`-ом из исходников по замороженному тегу (URL на tag-архив + SHA256; каталог `_deps/` в дереве build'а).
  vcpkg не используется.
    - **Linux**: системные пакеты (`sudo apt install libsdl2-dev libsdl2-ttf-dev libspdlog-dev libgtest-dev`) — configure
      мгновенный, ничего не качается.
    - **Windows/MinGW**: системных пакетов нет → deps скачиваются и собираются из исходников при первом configure
      (SDL2, SDL2_ttf, freetype, spdlog, googletest; ~5–15 мин один раз). Fetched-дерево кэшируется в
      `cmake-build-*/_deps/`, пересборка не качает заново.
    - **Windows: первый configure хочет доверенные CA-сертификаты.** FetchContent качает через встроенный CMake-curl
      (GnuTLS), которому на Windows неоткуда взять trust anchors → качалка падает (`SSL certificate verification failed ...
      no trust anchors configured`). Перед первым `cmake --preset ...` задайте системный CA-бандл, например Git-овский:
      `$env:SSL_CERT_FILE = "C:\Program Files\Git\usr\ssl\certs\ca-bundle.crt"`. Не выключайте проверку через
      `-DCMAKE_TLS_VERIFY=OFF`.
    - **`_deps/` живёт внутри каждого бинарьного каталога** (`cmake-build-debug/_deps`, `cmake-build-release/_deps`), а не в
      общем корне: у FetchContent каталог `-build` (с `CMAKE_BUILD_TYPE` «кто первый») лежит тоже в базовом каталоге.
      Общий базовый корень заставил бы release-сборку линковать Debug-депсы (искажает калибровку бенчмарка) и
      конфликтовал бы при смене тулчейна. Цена — одноразовая перекачка/пересборка в каждом каталоге (~5–15 мин); при
      желании можно вынести качалку в общий корень через `SOURCE_DIR` в `FetchContent_Declare`, оставив `BINARY_DIR` в
      своём бинарьном каталоге (намеренно не сделано по умолчанию).
- Конфигурация — через `CMakePresets.json`: `debug`/`release` (Windows) и `linux-debug`/`linux-release` (Linux);
  binaryDir'ы совпадают с CLion-овскими (`cmake-build-{debug,release}/`, gitignored). CLion работает как раньше (свои
  профили, те же каталоги); VS Code — расширение CMake Tools (пресеты подхватываются автоматически) + clangd.

```powershell
# Windows: configure + build + tests
cmake --preset debug            # или release; на Linux: linux-debug / linux-release
cmake --build cmake-build-debug
ctest --test-dir cmake-build-debug
# или напрямую (GTest-тесты автодискаверятся через gtest_discover_tests)
./cmake-build-debug/bin/DxvUITests.exe

# пересборка по существующему кэшу — просто build:
cmake --build cmake-build-release
```

- Transfertные флаги:
    - `-DDXVUI_FORCE_FETCH_DEPS=ON` — игнорировать систему и тянуть всё из исходников (проверка fetch-пути/CI);
    - `-DFETCHCONTENT_FULLY_DISCONNECTED=ON` — офлайн-переконфигурация из уже скачанного `_deps` (без сети).
- SDL_ttf поверх фетчнутого freetype: `cmake/deps.cmake` тянет freetype и подкладывает обёртку
  `cmake/FindFreetype.cmake`, редиректящую `Freetype::Freetype` на собранный таргет (активируется только вокруг
  сборки SDL_ttf, системный поиск Linux не затрагивает).

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
  `target_link_libraries(app PRIVATE DxvUI::DxvUI)`; публичные зависимости (SDL2/SDL2_ttf/spdlog) подтягиваются
  через `find_dependency`.
- Упаковка/export доступна только когда зависимости пришли из системы (импортированные таргеты). При
  FetchContent-фетче (`cmake/deps.cmake` ставит `DXVUI_DEPS_EXPORTABLE=FALSE`) экспорт пропускается — реальные таргеты
  из `_deps` не попадают в экспорт-сет; собирается только статика + headers.
- Потребитель должен иметь собственные SDL2/SDL2_ttf/spdlog (консольному приложению нужен
  `#define SDL_MAIN_HANDLED` до включения заголовков DxvUI — зонтичный
  `DxvUI/DxvUI.h` тянет `<SDL.h>`, который на Windows редиректит `main` → `SDL_main`).
- Dev-experience в репо: в `bin/` рядом с экзешниками копируются MinGW-runtime DLL
  (`libstdc++-6/libgcc_s_seh-1/libwinpthread-1`, таргет `dxvui_mingw_runtime`) и рантайм-DLL фетчнутых
  SDL2/SDL2_ttf (`dxvui_deploy_fetched_runtime_dlls`, см. `cmake/deps.cmake`) — собранные бинарники запускаются двойным
  кликом без настройки PATH.

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
- **Один целостный тулчейн — и бинарники самодостаточны.** FetchContent-депсы (SDL2, SDL2_ttf, freetype, spdlog,
  googletest) собираются тем же компилятором, что и проект. MinGW-runtime и DLL депсов кладутся в `bin/` из этой же
  сборки (`dxvui_mingw_runtime` + `dxvui_deploy_fetched_runtime_dlls`: SDL2, SDL2_ttf, spdlog, freetype), поэтому
  экзешники запускаются двойным кликом без настройки PATH; пересечения разных рантаймов не возникает. Старый сценарий
  vcpkg-времён (exe с `libspdlogd.dll` грузил чужой `libstdc++-6.dll` из PATH → `0xC0000139`) больше не воспроизводится.
- **Windows: следите, чтобы на PATH не стоял «скомканный» набор тулчейнов.** CMake берёт первый подходящий компилятор
  из PATH; соседство scoop `llvm` и `mingw-winlibs-ucrt` заставляет его выбрать `clang++`/`llvm-rc`, которые без Visual
  Studio нерабочие (detect broken, `winresrc.h not found`). Достаточно выстроить PATH нужного MinGW первым (в CLion он
  подставляется сам) либо задать компиляторы **по именам без путей**:
  `cmake --preset debug -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DCMAKE_RC_COMPILER=windres`. Жёсткий абсолютный
  путь (раньше этого требовал vcpkg) больше не нужен. Проверено на Windows: CLion GCC 15.2 (debug/release) и scoop
  `mingw-winlibs-ucrt` GCC 16.1 — 385/385 тестов.
- **MSVC-путь (fetch-сборка под `cl`)** не проверялся: код имеет ветки `/W4`, `dxvui_mingw_runtime` пропускается, но
  отдельного прогона не было.

## Conventions

- Formatting: `.clang-format` (ColumnLimit 100, indent 4, `PointerAlignment: Left`, includes sorted/regrouped). Project
  was bulk-formatted in commit `9846733`; run clang-format on touched files.
- `.clang-tidy` is generated from CLion inspections — do not hand-maintain.
- Headers under `include/DxvUI/`, impls in `src/` mirroring the same subdirs (`widgets/`, `containers/`, `interfaces/`,
  `backend/`, `text/`, `style/`, `layout/`). All `I*` abstractions live in `interfaces/`; all SDL/backend concrete
  implementations live in `backend/`.
- Layout/arrange logic was recently extracted from `SceneNode` into the container classes — put measure/arrange
  overrides in containers, not `SceneNode`.
- Commit messages and some comments are in Russian; match that when relevant.
