cmake_minimum_required(VERSION 3.20)
project(DxvUI VERSION 0.1.0 LANGUAGES CXX)

# --- Options ---
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)  # compile_commands.json для clangd
option(DXVUI_BUILD_EXAMPLES "Собирать примеры (DxvUI*Example, DxvUIBenchmark)" ON)
include(CTest)  # включает enable_testing() и даёт переменную BUILD_TESTING

# --- Find Dependencies via vcpkg ---
find_package(SDL2 REQUIRED)
find_package(SDL2_ttf REQUIRED)
find_package(sdl2-gfx REQUIRED)
find_package(spdlog CONFIG REQUIRED)

# --- Common ---
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

function(dxvui_enable_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

# --- DxvUI Library ---
add_library(DxvUI STATIC)
target_compile_features(DxvUI PUBLIC cxx_std_23)

target_sources(DxvUI PRIVATE
        src/Log.cpp

        src/EventManager.cpp
        src/DxvEvent.cpp
        src/Scene.cpp
        src/SceneNode.cpp
        src/UIBinding.cpp
        src/UIContext.cpp
        src/widgets/Button.cpp
        src/widgets/Checkbox.cpp
        src/widgets/Label.cpp
        src/widgets/TextEdit.cpp
        src/widgets/Popup.cpp
        src/containers/CenterContainer.cpp
        src/containers/AbsoluteContainer.cpp
        src/containers/HorizontalContainer.cpp
        src/layout/LayoutManager.cpp
        src/renderers/SDLRenderer.cpp
        src/renderers/SDLTextEngine.cpp
        src/renderers/SDLTextEditorView.cpp
        src/sources/SDLClipboard.cpp
        src/sources/SDLEventSource.cpp
        src/style/Color.cpp
        src/style/StyleManager.cpp
        src/style/Theme.cpp
        src/text/TextEditor.cpp
)

target_include_directories(DxvUI PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Internal headers (e.g. renderers/SDLTexture.h) reachable by the library's own
# sources via the public-style include path, but not installed or exposed.
target_include_directories(DxvUI PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(DxvUI PUBLIC
        SDL2::SDL2
        SDL2::SDL2_gfx
        SDL2_ttf::SDL2_ttf
        spdlog::spdlog
)

dxvui_enable_warnings(DxvUI)

# --- Runtime DLL deployment (Windows/MinGW dev-experience) ---
# vcpkg's applocal already copies its own DLLs next to the executables; this
# adds the MinGW runtime (imported by our exes AND by libspdlog.dll), so the
# binaries in bin/ run standalone without a toolchain-specific PATH.
# One shared target instead of per-exe POST_BUILD: concurrent copies of the
# same DLLs into the shared bin/ race on Windows (sharing violation).
if(WIN32 AND NOT MSVC)
    get_filename_component(DXVUI_MINGW_BIN "${CMAKE_CXX_COMPILER}" DIRECTORY)
    add_custom_target(dxvui_mingw_runtime
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${DXVUI_MINGW_BIN}/libstdc++-6.dll"
            "${DXVUI_MINGW_BIN}/libgcc_s_seh-1.dll"
            "${DXVUI_MINGW_BIN}/libwinpthread-1.dll"
            "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
endif()

function(dxvui_deploy_runtime_dlls target)
    if(TARGET dxvui_mingw_runtime)
        add_dependencies(${target} dxvui_mingw_runtime)
    endif()
endfunction()

# --- Install / packaging ---
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(DXVUI_INSTALL_CMAKEDIR ${CMAKE_INSTALL_LIBDIR}/cmake/DxvUI)

install(TARGETS DxvUI EXPORT DxvUITargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

configure_package_config_file(
        cmake/DxvUIConfig.cmake.in
        "${CMAKE_CURRENT_BINARY_DIR}/DxvUIConfig.cmake"
        INSTALL_DESTINATION ${DXVUI_INSTALL_CMAKEDIR})
write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/DxvUIConfigVersion.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion)
install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/DxvUIConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/DxvUIConfigVersion.cmake"
        DESTINATION ${DXVUI_INSTALL_CMAKEDIR})
install(EXPORT DxvUITargets
        FILE DxvUITargets.cmake
        NAMESPACE DxvUI::
        DESTINATION ${DXVUI_INSTALL_CMAKEDIR})

# Build-tree export: consumers can find_package(DxvUI) against the build dir
# (pass -DDxvUI_DIR=<builddir>) without installing.
export(EXPORT DxvUITargets
        FILE "${CMAKE_CURRENT_BINARY_DIR}/cmake/DxvUITargets.cmake"
        NAMESPACE DxvUI::)

# --- Examples ---
function(add_dxvui_example name src)
    add_executable(${name} ${src})
    target_link_libraries(${name} PRIVATE DxvUI SDL2::SDL2main)
    dxvui_enable_warnings(${name})
    dxvui_deploy_runtime_dlls(${name})
endfunction()

if(DXVUI_BUILD_EXAMPLES)
    add_dxvui_example(DxvUIExample examples/main.cpp)
    add_dxvui_example(DxvUIAlignmentExample examples/alignment.cpp)
    add_dxvui_example(DxvUIEventsExample examples/events.cpp)
    add_dxvui_example(DxvUITextEditExample examples/textedit.cpp)
    add_dxvui_example(DxvUIPopupExample examples/popup.cpp)
    add_dxvui_example(DxvUIBenchmark examples/benchmark.cpp)
endif()

# --- Tests ---
if(BUILD_TESTING)
    find_package(GTest REQUIRED)

    add_executable(DxvUITests
            tests/CheckboxTests.cpp
            tests/ColorTests.cpp
            tests/EventManagerTests.cpp
            tests/FpsCounterTests.cpp
            tests/LayoutManagerTests.cpp
            tests/LogTests.cpp
            tests/StyleManagerTests.cpp
            tests/WidgetTests.cpp
            tests/TextEditorTests.cpp
            tests/TextEngineTests.cpp
            tests/SDLEventSourceTests.cpp
            tests/SceneTests.cpp
            tests/PopupTests.cpp
            tests/UIBindingTests.cpp
            tests/UIContextTests.cpp
    )

    target_link_libraries(DxvUITests PRIVATE
            DxvUI
            GTest::gtest_main
    )

    dxvui_enable_warnings(DxvUITests)
    dxvui_deploy_runtime_dlls(DxvUITests)

    include(GoogleTest)
    gtest_discover_tests(DxvUITests)
endif()

# Копия compile_commands.json в корень репозитория — clangd (VS Code) ищет её там.
add_custom_target(copy_compile_commands ALL
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_BINARY_DIR}/compile_commands.json
        ${CMAKE_SOURCE_DIR}/compile_commands.json)
