find_program(GLSLANG
  NAMES glslang glslangValidator
  PATHS $ENV{VULKAN_SDK}/bin $ENV{VULKAN_SDK}/bin32
)

macro(add_spirv_shader GLSL_SOURCE_FILE)
  set(oneValueArgs OUTPUT EMBED)
  set(multiValueArgs "")
  cmake_parse_arguments(SPIRV_SHADER "" "${oneValueArgs}" "${multiValueArgs}" ${ARGN} )

  # Check args.
  if(NOT DEFINED SPIRV_SHADER_OUTPUT)
    message(FATAL_ERROR "You must set OUTPUT to compile GLSL to SPIR-V.")
  endif()

  # Get absolute path to GLSL.
  get_filename_component(GLSL_SOURCE_FILE_ABS ${GLSL_SOURCE_FILE} ABSOLUTE)

  # Get directory of output SPIR-V for creating the dir before compilation.
  get_filename_component(OUTPUT_DIR ${SPIRV_SHADER_OUTPUT} DIRECTORY)

  # Mark output as generated file.
  set_source_files_properties(${SPIRV_SHADER_OUTPUT} PROPERTIES GENERATED TRUE)

  set(GLSLANG_FLAGS "")

  # Embedding SPIR-V blob into the artifact.
  if(DEFINED SPIRV_SHADER_EMBED)
    set(GLSLANG_FLAGS ${GLSLANG_FLAGS} --vn ${SPIRV_SHADER_EMBED})
  endif()

  add_custom_command(
    MAIN_DEPENDENCY ${GLSL_SOURCE_FILE}
    OUTPUT ${SPIRV_SHADER_OUTPUT}
    COMMAND ${CMAKE_COMMAND} -E make_directory "${OUTPUT_DIR}"
    COMMAND ${GLSLANG} ${GLSLANG_FLAGS} -V --target-env vulkan1.0 -o ${SPIRV_SHADER_OUTPUT} ${GLSL_SOURCE_FILE_ABS}
    DEPENDS ${GLSL_SOURCE_FILE}
  )
endmacro(add_spirv_shader)

macro(add_spirv_shader_lib TARGET_NAME)
  set(options EMBED)
  set(oneValueArgs OUTPUT)
  set(multiValueArgs SOURCES DEPENDS)
  cmake_parse_arguments(SPIRV_SHADER_LIB "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN} )

  get_filename_component(OUTPUT_PATH ${SPIRV_SHADER_LIB_OUTPUT} ABSOLUTE)

  #get_filename_component(OUTPUT_DIR ${SPIRV_SHADER_LIB_OUTPUT} DIRECTORY)
  #get_filename_component(OUTPUT_DIR_ABS ${OUTPUT_DIR} ABSOLUTE)
  #set(INCLUDE_LINES "")
  #set(HEADER_FILES "")
  #foreach (GLSL_FILE ${SPIRV_SHADER_LIB_SOURCES})
  #  get_filename_component(GLSL_FILE_NAME ${GLSL_FILE_NAME} NAME)
  #  set(HEADER_FILE ${OUTPUT_DIR_ABS}/${GLSL_FILE}.spv.h)
  #  add_spirv_shader(
  #    ${GLSL_FILE}
  #    OUTPUT ${HEADER_FILE}
  #    EMBED ${SHADER}_spirv
  #  )
  #  set(INCLUDES "${INCLUDES}\n#include <spirv/${SHADER}.spv.h>")
  #  set(SHADERS ${SHADERS} ${SHADER}.comp)
  #  set(HEADERS ${HEADERS} ${RadeonRays_BINARY_DIR}/spirv/${SHADER}.spv.h)
  #endforeach

  set(SPIRV_FILES "")
  foreach (GLSL_FILE ${SPIRV_SHADER_LIB_SOURCES})
    get_filename_component(FILE_NAME ${GLSL_FILE} NAME)
    set(SPIRV_FILE "${OUTPUT_PATH}/${FILE_NAME}.spv")
    add_spirv_shader(
      ${GLSL_FILE}
      OUTPUT ${SPIRV_FILE}
    )
    list(APPEND SPIRV_FILES ${SPIRV_FILE})
  endforeach()

  add_custom_target(${TARGET_NAME}
    SOURCES ${SPIRV_SHADER_LIB_SOURCES}
    DEPENDS ${SPIRV_FILES} ${SPIRV_SHADER_LIB_DEPENDS}
  )
endmacro()
