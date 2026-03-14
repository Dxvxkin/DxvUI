cmake_minimum_required(VERSION 3.16)
project(DxvUI)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Fetch Dependencies ---
include(FetchContent)

FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

FetchContent_Declare(
  SDL2
  GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
  GIT_TAG release-2.28.5
)
FetchContent_MakeAvailable(SDL2)


# --- DxvUI Library ---
add_library(DxvUI STATIC)

target_sources(DxvUI PRIVATE
    src/SceneNode.cpp
    src/WidgetFactory.cpp
    src/widgets/Button.cpp
    src/containers/CenterContainer.cpp
    src/renderers/SDLRenderer.cpp
)

target_include_directories(DxvUI PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Link DxvUI to its dependencies
# We also add SDL2main here so any executable linking DxvUI gets it.
target_link_libraries(DxvUI PUBLIC
    nlohmann_json::nlohmann_json
    SDL2::SDL2-static
    SDL2::SDL2main # This provides the WinMain entry point
)


# --- Example ---
# The WIN32 flag is removed for now to keep the console for debugging.
add_executable(DxvUIExample examples/main.cpp)

# Link the example to our library
target_link_libraries(DxvUIExample PRIVATE DxvUI)
