cmake_minimum_required(VERSION 3.15)
project(DxvUI)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Fetch nlohmann_json ---
include(FetchContent)
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

# --- DxvUI Library ---
add_library(DxvUI STATIC)

target_sources(DxvUI PRIVATE
    src/SceneNode.cpp
    src/WidgetFactory.cpp
    src/widgets/Button.cpp
    src/containers/CenterContainer.cpp
)

target_include_directories(DxvUI PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(DxvUI PUBLIC nlohmann_json::nlohmann_json)

# --- Example ---
add_executable(DxvUIExample examples/main.cpp)
target_link_libraries(DxvUIExample PRIVATE DxvUI)

# --- Optional: Installation ---
# Allows you to install the library system-wide or in a local directory
# include(GNUInstallDirs)
# install(TARGETS DxvUI
#     EXPORT DxvUI-targets
#     ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
#     LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
# )
# install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
