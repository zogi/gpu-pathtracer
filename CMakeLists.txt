cmake_minimum_required(VERSION 3.8)

project(GpuPathTracer CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(cmake/glslang.cmake)

# Vulkan
find_package(Vulkan REQUIRED)

# SPIRV-Reflect
add_library(spirv_reflect STATIC
  external/SPIRV-Reflect/spirv_reflect.c
)
target_include_directories(spirv_reflect SYSTEM PUBLIC
  external/SPIRV-Reflect
)

# GSL
add_subdirectory(external/GSL)

# CPU bvh builder parts of RadeonRays.
# Files not under RADEONRAYS_ROOT are copied to this repo and may be modified.
set(RADEONRAYS_ROOT external/RadeonRays_SDK/RadeonRays)
add_library(RadeonRaysBVH STATIC
  ${RADEONRAYS_ROOT}/include/radeon_rays.h
  ${RADEONRAYS_ROOT}/include/math/bbox.h
  ${RADEONRAYS_ROOT}/include/math/float2.h
  ${RADEONRAYS_ROOT}/include/math/float3.h
  ${RADEONRAYS_ROOT}/include/math/int2.h
  ${RADEONRAYS_ROOT}/include/math/int3.h
  ${RADEONRAYS_ROOT}/include/math/mathutils.h
  ${RADEONRAYS_ROOT}/include/math/matrix.h
  ${RADEONRAYS_ROOT}/include/math/quaternion.h
  ${RADEONRAYS_ROOT}/include/math/ray.h
  ${RADEONRAYS_ROOT}/include/math/ray.h
  ${RADEONRAYS_ROOT}/src/api/radeon_rays_impl.cpp
  ${RADEONRAYS_ROOT}/src/accelerator/bvh.h
  ${RADEONRAYS_ROOT}/src/accelerator/bvh.cpp
  ${RADEONRAYS_ROOT}/src/accelerator/split_bvh.h
  ${RADEONRAYS_ROOT}/src/accelerator/split_bvh.cpp
  ${RADEONRAYS_ROOT}/src/except/except.h
  ${RADEONRAYS_ROOT}/src/primitive/instance.h
  ${RADEONRAYS_ROOT}/src/primitive/mesh.h
  ${RADEONRAYS_ROOT}/src/primitive/mesh.cpp
  ${RADEONRAYS_ROOT}/src/primitive/shapeimpl.h
  ${RADEONRAYS_ROOT}/src/util/options.h
  ${RADEONRAYS_ROOT}/src/util/options.cpp
  ${RADEONRAYS_ROOT}/src/world/world.h
  ${RADEONRAYS_ROOT}/src/world/world.cpp
  bvh/RadeonRays/plain_bvh_translator.h
  bvh/RadeonRays/plain_bvh_translator.cpp
  bvh/RadeonRays/intersector_skip_links.h
  bvh/RadeonRays/intersector_skip_links.cpp
)
target_compile_definitions(RadeonRaysBVH PUBLIC RR_STATIC_LIBRARY)
target_include_directories(RadeonRaysBVH PUBLIC
  ${RADEONRAYS_ROOT}/include
  ${RADEONRAYS_ROOT}/src
  bvh
)
target_link_libraries(RadeonRaysBVH GSL)

# # stb
# add_library(stb INTERFACE)
# target_include_directories(stb SYSTEM INTERFACE
#   ${CMAKE_CURRENT_LIST_DIR}/external/stb
# )

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

# # glfw
# option(GLFW_BUILD_EXAMPLES "" OFF)
# option(GLFW_BUILD_TESTS "" OFF)
# option(GLFW_BUILD_DOCS "" OFF)
# option(GLFW_INSTALL "" OFF)
# option(GLFW_VULKAN_STATIC "" OFF)
# option(GLFW_DOCUMENT_INTERNALS "" OFF)
# add_subdirectory(external/glfw)

# imgui
file(GLOB IMGUI_SRC external/imgui/*.cpp)
add_library(imgui STATIC ${IMGUI_SRC})
target_include_directories(imgui SYSTEM PUBLIC external/imgui)

# windowlib
add_library(windowlib STATIC
  ui/imgui_impl_glfw.h
  ui/imgui_impl_glfw.cpp
  ui/imgui_impl_vulkan.h
  ui/imgui_impl_vulkan.cpp
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
  shaders/RadeonRays/bvh.glslh
)

# # Granite
# option(GRANITE_VULKAN_FOSSILIZE "" ON)
# option(GLFW_BUILD_EXAMPLES "" OFF)
# option(GLFW_BUILD_TESTS "" OFF)
# option(GLFW_BUILD_DOCS "" OFF)
# option(GLFW_INSTALL "" OFF)
# option(GLFW_VULKAN_STATIC "" OFF)
# option(GLFW_DOCUMENT_INTERNALS "" OFF)
set(SPIRV_HEADERS_SKIP_EXAMPLES ON CACHE STRING "" FORCE)
set(SKIP_SPIRV_TOOLS_INSTAL OFF CACHE STRING "" FORCE)
set(SHADERC_SKIP_TESTS ON CACHE STRING "" FORCE)
set(GRANITE_VULKAN_FOSSILIZE OFF CACHE STRING "Vulkan Fossilize" FORCE)
set(GRANITE_VULKAN_MT OFF CACHE STRING "Vulkan MT" FORCE)
set(GRANITE_VULKAN_FILESYSTEM OFF CACHE STRING "Vulkan filesystem" FORCE)
add_subdirectory(external/granite EXCLUDE_FROM_ALL)
target_include_directories(granite PUBLIC external)

# Main
add_executable(GpuPathTracer WIN32
  # main.cpp
  main_granite.cpp
  camera.h
  camera.cpp
  common.h
  shaders/quad.vert
  ${PROJECT_BINARY_DIR}/include/shaders/quad.vert.spv.h
  shaders/depth.frag
  ${PROJECT_BINARY_DIR}/include/shaders/depth.frag.spv.h
  shaders/RadeonRays/bvh.glslh
)
target_compile_definitions(GpuPathTracer PRIVATE
  # For running the granite RenderGraph sandbox.
  ASSET_DIRECTORY="${PROJECT_SOURCE_DIR}/external/granite/tests/assets"
)
target_include_directories(GpuPathTracer PRIVATE
  ${PROJECT_BINARY_DIR}/include
)
target_link_libraries(GpuPathTracer
  granite
  granite-application-entry
  RadeonRaysBVH
  spirv_reflect
  tinyobjloader
  windowlib
  glm
  stb
  GSL
)
