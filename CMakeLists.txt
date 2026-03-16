cmake_minimum_required(VERSION 3.16)
project(DxvUI)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Find Dependencies via vcpkg ---
# Using specific names as per user's environment
find_package(nlohmann_json CONFIG REQUIRED)
find_package(SDL2 REQUIRED)
find_package(sdl2-gfx REQUIRED)
find_package(GTest REQUIRED)


# --- DxvUI Library ---
add_library(DxvUI STATIC)

target_sources(DxvUI PRIVATE
    src/ActionRegistry.cpp
    src/Color.cpp
    src/Scene.cpp
    src/SceneNode.cpp
    # src/WidgetFactory.cpp
    src/widgets/Button.cpp
    src/containers/CenterContainer.cpp
    src/renderers/SDLRenderer.cpp
    src/sources/SDLEventSource.cpp
    src/visuals/ButtonVisual.cpp
)

target_include_directories(DxvUI PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(DxvUI PUBLIC
    nlohmann_json::nlohmann_json
    SDL2::SDL2
    SDL2::SDL2_gfx # Using specific target name as per user's environment
)


# --- Example ---
add_executable(DxvUIExample examples/main.cpp)
target_link_libraries(DxvUIExample PRIVATE
    DxvUI
    SDL2::SDL2main
)


# --- Tests ---
enable_testing()
add_executable(DxvUITests
    tests/ColorTests.cpp
)
target_link_libraries(DxvUITests PRIVATE
    DxvUI
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(DxvUITests)
