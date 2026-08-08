cmake_minimum_required(VERSION 3.16)
project(DxvUI)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- Find Dependencies via vcpkg ---
#find_package(nlohmann_json CONFIG REQUIRED)
find_package(SDL2 REQUIRED)
find_package(SDL2_ttf REQUIRED)
find_package(sdl2-gfx REQUIRED)
find_package(GTest REQUIRED)

find_package(spdlog CONFIG REQUIRED)


# --- DxvUI Library ---
add_library(DxvUI STATIC)

target_sources(DxvUI PRIVATE
        src/Log.cpp # Add the new Log source file

        src/EventManager.cpp
        src/DxvEvent.cpp
        src/Scene.cpp
        src/SceneNode.cpp
        src/UIBinding.cpp
        src/UIContext.cpp
        src/widgets/Button.cpp
        src/widgets/Label.cpp
        src/containers/CenterContainer.cpp
        src/containers/AbsoluteContainer.cpp
        src/containers/HorizontalContainer.cpp
        src/layout/LayoutManager.cpp
        src/renderers/SDLRenderer.cpp
        src/renderers/SDLTextEngine.cpp
        src/sources/SDLEventSource.cpp
        src/style/Color.cpp
        src/style/StyleManager.cpp
        src/style/Theme.cpp

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
        #        nlohmann_json::nlohmann_json
        SDL2::SDL2
        SDL2::SDL2_gfx
        SDL2_ttf::SDL2_ttf
        spdlog::spdlog
)

# --- Example ---
add_executable(DxvUIExample examples/main.cpp)
target_link_libraries(DxvUIExample PRIVATE
        DxvUI
        SDL2::SDL2main
)

# --- Alignment example ---
add_executable(DxvUIAlignmentExample examples/alignment.cpp)
target_link_libraries(DxvUIAlignmentExample PRIVATE
        DxvUI
        SDL2::SDL2main
)

# --- Events example ---
add_executable(DxvUIEventsExample examples/events.cpp)
target_link_libraries(DxvUIEventsExample PRIVATE
        DxvUI
        SDL2::SDL2main
)

# --- Benchmark ---
add_executable(DxvUIBenchmark examples/benchmark.cpp)
target_link_libraries(DxvUIBenchmark PRIVATE
        DxvUI
        SDL2::SDL2main
)


# --- Tests ---
enable_testing()
add_executable(DxvUITests
        tests/ColorTests.cpp
        tests/EventManagerTests.cpp
        tests/FpsCounterTests.cpp
        tests/LayoutManagerTests.cpp
        tests/LogTests.cpp
        tests/StyleManagerTests.cpp
        tests/WidgetTests.cpp
        tests/TextEngineTests.cpp
        tests/UIBindingTests.cpp
        tests/UIContextTests.cpp

)
target_link_libraries(DxvUITests PRIVATE
        DxvUI
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(DxvUITests)
