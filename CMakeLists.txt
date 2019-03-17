cmake_minimum_required(VERSION 3.8)

project(GpuPathTracer CXX)

include(cmake/glslang.cmake)

# Vulkan
find_package(Vulkan REQUIRED)

if (WIN32)
  # Hack to make Anvil find vulkan-1.lib
  add_library(vulkan-1.lib INTERFACE)
  set_target_properties(vulkan-1.lib PROPERTIES
    INTERFACE_LINK_LIBRARIES "${VULKAN_LIBRARY}")
endif()

# Radeon Rays
option(RR_USE_VULKAN "" ON)
option(RR_USE_OPENCL "" OFF)
option(RR_USE_EMBREE "" OFF)
option(RR_EMBED_KERNELS "" OFF)
option(RR_ALLOW_CPU_DEVICES "" OFF)
option(RR_NO_TESTS "" ON)
option(RR_ENABLE_STATIC "" ON)
option(RR_SHARED_CALC "" OFF)
option(RR_ENABLE_RAYMASK "" OFF)
option(RR_ENABLE_BACKFACE_CULL "" OFF)
add_subdirectory(external/RadeonRays_SDK)

# stb
add_library(stb INTERFACE)
set_target_properties(stb PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_CURRENT_LIST_DIR}/external/stb
)

# Tiny OBJ loader
option(TINYOBJLOADER_USE_DOUBLE "" OFF)
option(TINYOBJLOADER_BUILD_TEST_LOADER "" OFF)
option(TINYOBJLOADER_BUILD_OBJ_STICHER "" OFF)
add_subdirectory(external/tinyobjloader)

# glm
add_library(glm INTERFACE)
target_include_directories(glm INTERFACE
    ${PROJECT_SOURCE_DIR}/external/glm)

# glfw
option(GLFW_BUILD_EXAMPLES "" OFF)
option(GLFW_BUILD_TESTS "" OFF)
option(GLFW_BUILD_DOCS "" OFF)
option(GLFW_INSTALL "" OFF)
option(GLFW_VULKAN_STATIC "" OFF)
option(GLFW_DOCUMENT_INTERNALS "" OFF)
add_subdirectory(external/glfw)

# imgui
file(GLOB IMGUI_SRC external/imgui/*.cpp)
add_library(imgui STATIC ${IMGUI_SRC})
target_include_directories(imgui PUBLIC external/imgui)

# windowlib
add_library(windowlib STATIC
  external/imgui/examples/imgui_impl_glfw.h
  external/imgui/examples/imgui_impl_glfw.cpp
  external/imgui/examples/imgui_impl_vulkan.h
  external/imgui/examples/imgui_impl_vulkan.cpp
  glad/src/glad.c
)
target_link_libraries(windowlib imgui glfw Vulkan::Vulkan)
target_compile_definitions(windowlib PUBLIC
  IMGUI_IMPL_OPENGL_LOADER_GLAD
)
target_include_directories(windowlib PUBLIC
  external/imgui/examples
  glad/include
)

set(GLFW_VULKAN_DIR external/imgui/examples/example_glfw_vulkan)
add_spirv_shader_lib(imgui_shaders
  SOURCES
    ${GLFW_VULKAN_DIR}/glsl_shader.vert
    ${GLFW_VULKAN_DIR}/glsl_shader.frag
  OUTPUT
    shaders/imgui
)
add_dependencies(windowlib imgui_shaders)

# Main
add_executable(GpuPathTracer
  main.cpp
  camera.h
  camera.cpp
)
target_link_libraries(GpuPathTracer
  RadeonRays stb tinyobjloader windowlib glm
)
