cmake_minimum_required(VERSION 3.20)
project(DxvUI LANGUAGES CXX)

# --- Options ---
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

# --- Examples ---
function(add_dxvui_example name src)
    add_executable(${name} ${src})
    target_link_libraries(${name} PRIVATE DxvUI SDL2::SDL2main)
    dxvui_enable_warnings(${name})
endfunction()

if(DXVUI_BUILD_EXAMPLES)
    add_dxvui_example(DxvUIExample examples/main.cpp)
    add_dxvui_example(DxvUIAlignmentExample examples/alignment.cpp)
    add_dxvui_example(DxvUIEventsExample examples/events.cpp)
    add_dxvui_example(DxvUITextEditExample examples/textedit.cpp)
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
            tests/UIBindingTests.cpp
            tests/UIContextTests.cpp
    )

    target_link_libraries(DxvUITests PRIVATE
            DxvUI
            GTest::gtest_main
    )

    dxvui_enable_warnings(DxvUITests)

    include(GoogleTest)
    gtest_discover_tests(DxvUITests)
endif()
