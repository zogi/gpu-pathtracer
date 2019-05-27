cmake_minimum_required(VERSION 3.8)

project(GpuPathTracer CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(cmake/glslang.cmake)

# Vulkan
find_package(Vulkan REQUIRED)

if (WIN32)
  # Hack to make Anvil find vulkan-1.lib
  add_library(vulkan-1.lib INTERFACE)
  set_target_properties(vulkan-1.lib PROPERTIES
    INTERFACE_LINK_LIBRARIES "${VULKAN_LIBRARY}")
endif()

# SPIRV-Reflect
add_library(spirv_reflect STATIC
  external/SPIRV-Reflect/spirv_reflect.c
)
target_include_directories(spirv_reflect SYSTEM PUBLIC
  external/SPIRV-Reflect
)

# Radeon Rays
option(RR_USE_VULKAN "" ON)
option(RR_USE_OPENCL "" OFF)
option(RR_USE_EMBREE "" OFF)
option(RR_EMBED_KERNELS "" ON)
option(RR_ALLOW_CPU_DEVICES "" OFF)
option(RR_NO_TESTS "" ON)
option(RR_ENABLE_STATIC "" ON)
option(RR_SHARED_CALC "" OFF)
option(RR_ENABLE_RAYMASK "" OFF)
option(RR_ENABLE_BACKFACE_CULL "" OFF)
add_subdirectory(external/RadeonRays_SDK)

# GSL
add_subdirectory(external/GSL)

# stb
add_library(stb INTERFACE)
target_include_directories(stb SYSTEM INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/external/stb
)

# Tiny OBJ loader
option(TINYOBJLOADER_USE_DOUBLE "" OFF)
option(TINYOBJLOADER_BUILD_TEST_LOADER "" OFF)
option(TINYOBJLOADER_BUILD_OBJ_STICHER "" OFF)
add_subdirectory(external/tinyobjloader)

# glm
add_library(glm INTERFACE)
target_include_directories(glm SYSTEM INTERFACE
  ${PROJECT_SOURCE_DIR}/external/glm
)

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
target_include_directories(imgui SYSTEM PUBLIC external/imgui)

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
target_include_directories(windowlib SYSTEM PUBLIC
  external/imgui/examples
  glad/include
)

set(GLFW_VULKAN_DIR external/imgui/examples/example_glfw_vulkan)
add_spirv_shader_lib(imgui_shaders
  SOURCES
    ${GLFW_VULKAN_DIR}/glsl_shader.vert
    ${GLFW_VULKAN_DIR}/glsl_shader.frag
  OUTPUT
    ${PROJECT_BINARY_DIR}/shaders/imgui
)
add_dependencies(windowlib imgui_shaders)

add_spirv_shader(
  shaders/quad.vert
  OUTPUT ${PROJECT_BINARY_DIR}/include/shaders/quad.vert.spv.h
  EMBED quad_vert_spirv
)
add_spirv_shader(
  shaders/depth.frag
  OUTPUT ${PROJECT_BINARY_DIR}/include/shaders/depth.frag.spv.h
  EMBED depth_frag_spirv
)
source_group("shaders" FILES
  shaders/quad.vert
  shaders/depth.frag
)

# Main
add_executable(GpuPathTracer
  main.cpp
  camera.h
  camera.cpp
  common.h
  shaders/quad.vert
  ${PROJECT_BINARY_DIR}/include/shaders/quad.vert.spv.h
  shaders/depth.frag
  ${PROJECT_BINARY_DIR}/include/shaders/depth.frag.spv.h
)
target_include_directories(GpuPathTracer PRIVATE
  ${PROJECT_BINARY_DIR}/include
)
target_link_libraries(GpuPathTracer
  RadeonRays Anvil stb tinyobjloader windowlib glm spirv_reflect GSL
)
