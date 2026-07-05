cmake_minimum_required(VERSION 3.16)
project(pioneer_sync_gui LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Dependencies via FetchContent ──────────────────────────────────────────────
include(FetchContent)

FetchContent_Declare(glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
)
set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL        OFF CACHE BOOL "" FORCE)
# Disable optional display backends that may not be available on all systems
set(GLFW_BUILD_WAYLAND  OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.91.6
)
FetchContent_MakeAvailable(imgui)

# ImGui compiled as a static library with GLFW + OpenGL3 backend
add_library(imgui_backend STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui_backend PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui_backend PUBLIC glfw)
if(WIN32)
    target_link_libraries(imgui_backend PUBLIC opengl32)
elseif(APPLE)
    find_library(OPENGL_LIB OpenGL REQUIRED)
    target_link_libraries(imgui_backend PUBLIC ${OPENGL_LIB})
else()
    target_link_libraries(imgui_backend PUBLIC GL)
endif()

# ── prodjlink library (referenced as a sibling directory) ──────────────────────
add_subdirectory(../prodjlink prodjlink_build
    EXCLUDE_FROM_ALL  # don't build tests/examples from this context
)

# Disable prodjlink tests and examples when included from here
set(PRODJLINK_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(PRODJLINK_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

# ── Main executable ────────────────────────────────────────────────────────────
if(WIN32)
    # WIN32 flag removes the background console window on Windows
    add_executable(pioneer_sync_gui WIN32 main.cpp)
else()
    add_executable(pioneer_sync_gui main.cpp)
endif()

target_link_libraries(pioneer_sync_gui PRIVATE prodjlink imgui_backend)

# Access private prodjlink headers (network_interface.hpp) for interface listing
target_include_directories(pioneer_sync_gui PRIVATE ../prodjlink/src)
