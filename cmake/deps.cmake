# Политика «find-or-fetch» для зависимостей.
#
# Каждая зависимость сначала ищется в системе (find_package) — на Linux это
# apt-пакеты (libsdl2-dev, libsdl2-ttf-dev, libspdlog-dev, libgtest-dev),
# ничего не скачивается. Если зависимость не найдена (в первую очередь на
# Windows/MinGW), исходники тянутся через FetchContent по замороженному тегу
# (URL на tag-архив + контрольная сумма SHA256) и собираются в инструментом
# дереве build'а (${FETCHCONTENT_BASE_DIR:-builddir/_deps}).
#
# Флаги:
#   DXVUI_FORCE_FETCH_DEPS=ON  — игнорировать систему и принудительно тянуть
#                                всё из исходников (проверка fetch-пути/CI);
#   FETCHCONTENT_FULLY_DISCONNECTED=ON — офлайн-переконфигурация из уже
#                                скачанного _deps (без сети).

include(FetchContent)

if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)  # tar-архивы: фиксируем время извлечения
endif()

option(DXVUI_FORCE_FETCH_DEPS "Тянуть зависимости FetchContent-ом вместо системных пакетов" OFF)

set(DXVUI_DEPS_DIR "${CMAKE_CURRENT_LIST_DIR}")

# --- Версии (пины) ---
set(DXVUI_SDL2_VERSION "release-2.30.12")
set(DXVUI_SDL2_TTF_VERSION "release-2.24.0")
set(DXVUI_FREETYPE_VERSION "VER-2-13-3")
set(DXVUI_SPDLOG_VERSION "v1.17.0")
set(DXVUI_GTEST_VERSION "v1.18.0")

# --- SDL2 ---
if(NOT DXVUI_FORCE_FETCH_DEPS)
    find_package(SDL2 QUIET)
endif()
if(NOT SDL2_FOUND)
    message(STATUS "DxvUI/deps: SDL2 не найдена в системе — FetchContent (${DXVUI_SDL2_VERSION})")
    FetchContent_Declare(SDL
            URL "https://github.com/libsdl-org/SDL/archive/refs/tags/${DXVUI_SDL2_VERSION}.tar.gz"
            URL_HASH "SHA256=560da2e54dd8af933e35bd08fb1b6cf80d4f6938c67710fecf13b7e9bdd6c47e")
    set(SDL_SHARED ON CACHE BOOL "" FORCE)
    set(SDL_STATIC OFF CACHE BOOL "" FORCE)
    set(SDL_TEST OFF CACHE BOOL "" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    # Аудио не требуется; source-сборка против нового pipewire не собирается.
    set(SDL_PIPEWIRE OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(SDL)
    set(DXVUI_SDL2_FETCHED TRUE)
else()
    message(STATUS "DxvUI/deps: SDL2 из системы (${SDL2_VERSION})")
endif()

# --- SDL2_ttf ---
if(NOT DXVUI_FORCE_FETCH_DEPS)
    find_package(SDL2_ttf QUIET)
endif()

# freetype нужен только когда SDL2_ttf собирается из исходников: тянем свой
# пин (системную на Windows неоткуда взять; на Linux SDL2_ttf берётся из apt и
# freetype уже входит в неё).
if(NOT SDL2_ttf_FOUND AND NOT TARGET freetype)
    message(STATUS "DxvUI/deps: freetype не найдена — FetchContent (${DXVUI_FREETYPE_VERSION})")
    FetchContent_Declare(freetype
            URL "https://github.com/freetype/freetype/archive/refs/tags/${DXVUI_FREETYPE_VERSION}.tar.gz"
            URL_HASH "SHA256=bc5c898e4756d373e0d991bab053036c5eb2aa7c0d5c67e8662ddc6da40c4103")
    set(FT_DISABLE_ZLIB ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(freetype)
endif()

if(NOT SDL2_ttf_FOUND)
    message(STATUS "DxvUI/deps: SDL2_ttf не найдена в системе — FetchContent (${DXVUI_SDL2_TTF_VERSION})")
    # SDL2_ttf 2.24: SDL2 берётся по существующему таргету SDL2::SDL2 (собранному
    # выше), freetype — через find_package(Freetype). Подкладываем свою обёртку
    # FindFreetype, которая редиректит на наш фетчнутый таргет freetype.
    if(TARGET freetype)
        list(PREPEND CMAKE_MODULE_PATH "${DXVUI_DEPS_DIR}")
    endif()
    FetchContent_Declare(SDL_ttf
            URL "https://github.com/libsdl-org/SDL_ttf/archive/refs/tags/${DXVUI_SDL2_TTF_VERSION}.tar.gz"
            URL_HASH "SHA256=2c45241a56203a59d66ec6b4eae9457e5675fc609376566a257391fd29d341a2")
    FetchContent_MakeAvailable(SDL_ttf)
    if(TARGET freetype)
        list(REMOVE_ITEM CMAKE_MODULE_PATH "${DXVUI_DEPS_DIR}")
    endif()
    set(DXVUI_SDL2TTF_FETCHED TRUE)
else()
    message(STATUS "DxvUI/deps: SDL2_ttf из системы (${SDL2_ttf_VERSION})")
endif()

# --- spdlog ---
if(NOT DXVUI_FORCE_FETCH_DEPS)
    find_package(spdlog CONFIG QUIET)
endif()
if(NOT spdlog_FOUND)
    message(STATUS "DxvUI/deps: spdlog не найдена в системе — FetchContent (${DXVUI_SPDLOG_VERSION})")
    FetchContent_Declare(spdlog
            URL "https://github.com/gabime/spdlog/archive/refs/tags/${DXVUI_SPDLOG_VERSION}.tar.gz"
            URL_HASH "SHA256=d8862955c6d74e5846b3f580b1605d2428b11d97a410d86e2fb13e857cd3a744")
    set(SPDLOG_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(SPDLOG_BUILD_BENCH OFF CACHE BOOL "" FORCE)
    set(SPDLOG_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(spdlog)
    set(DXVUI_SPDLOG_FETCHED TRUE)
endif()

# --- googletest (только тесты) ---
if(BUILD_TESTING)
    if(NOT DXVUI_FORCE_FETCH_DEPS)
        find_package(GTest QUIET)
    endif()
    if(NOT GTest_FOUND)
        message(STATUS "DxvUI/deps: googletest не найдена в системе — FetchContent (${DXVUI_GTEST_VERSION})")
        FetchContent_Declare(googletest
                URL "https://github.com/google/googletest/archive/refs/tags/${DXVUI_GTEST_VERSION}.tar.gz"
                URL_HASH "SHA256=6e3191c1455468b3fc35a417fb565c1c5071aee1b7e7f85e30cf48a98d37d8b5")
        set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
        set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(googletest)
    endif()
endif()

# --- Деплой рантайм-DLL фетчнутых shared-библиотек в общий bin/ ---
# vcpkg раньше копировал свои DLL (applocal); теперь для фетчнутых SDL2/SDL2_ttf
# это делаем сами, постучавшись к тому же custom target'у, что и для MinGW-runtime.
function(dxvui_deploy_fetched_runtime_dlls target)
    if(NOT WIN32)
        return()
    endif()
    set(_dll_targets)
    if(DXVUI_SDL2_FETCHED)
        list(APPEND _dll_targets SDL2)
    endif()
    if(DXVUI_SDL2TTF_FETCHED)
        list(APPEND _dll_targets SDL2_ttf)
    endif()
    if(NOT _dll_targets)
        return()
    endif()
    set(_dll_paths "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
    foreach(name ${_dll_targets})
        list(PREPEND _dll_paths "$<TARGET_FILE:${name}>")
    endforeach()
    add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${_dll_paths})
endfunction()

# --- Пригодность для install/export пакета ---
# install(EXPORT)/export(EXPORT) в CMakeLists ссылаются на SDL2::SDL2,
# SDL2_ttf::SDL2_ttf, spdlog::spdlog. С импортированными (системными) таргетами
# это работает; с реальными таргетами из FetchContent — нет (нет в экспорт-сетах).
# Поэтому упаковка пакета доступна только когда ни одна зависимость не фетчилась.
set(DXVUI_DEPS_EXPORTABLE TRUE)
if(DXVUI_SDL2_FETCHED OR DXVUI_SDL2TTF_FETCHED OR DXVUI_SPDLOG_FETCHED)
    set(DXVUI_DEPS_EXPORTABLE FALSE)
endif()