if (WIN32)
  if(NOT EXISTS "$ENV{VULKAN_SDK}")
    message(SEND_ERROR "VULKAN_SDK envvar not set")
  endif()

  if (${CMAKE_HOST_SYSTEM_PROCESSOR} STREQUAL "AMD64")
    set(GLSL_VALIDATOR "$ENV{VULKAN_SDK}/Bin/glslangValidator.exe")
  else()
    set(GLSL_VALIDATOR "$ENV{VULKAN_SDK}/Bin32/glslangValidator.exe")
  endif()

else()
  set(GLSL_VALIDATOR glslangValidator)
endif()

macro(add_spirv_shader GLSL_SOURCE_FILE SPIRV_FILE)
  set(multiValueArgs DEPENDS)
  cmake_parse_arguments(SPIRV_SHADER "" "" "${multiValueArgs}" ${ARGN} )

  if (IS_ABSOLUTE(GLSL_SOURCE_FILE))
    set(GLSL_SOURCE_FILE_ABS "${GLSL_SOURCE_FILE}")
  else()
    set(GLSL_SOURCE_FILE_ABS "${PROJECT_SOURCE_DIR}/${GLSL_SOURCE_FILE}")
  endif()

  get_filename_component(DIR_NAME ${GLSL_SOURCE_FILE} DIRECTORY)
  add_custom_command(
    OUTPUT ${SPIRV_FILE}
    COMMAND ${CMAKE_COMMAND} -E make_directory "${DIR_NAME}"
    COMMAND ${GLSL_VALIDATOR} -V ${GLSL_SOURCE_FILE_ABS} -o ${SPIRV_FILE}
    DEPENDS ${GLSL_SOURCE_FILE} ${SPIRV_SHADER_DEPENDS}
    SOURCES ${GLSL_SOURCE_FILE}
  )
endmacro(add_spirv_shader)

macro(add_spirv_shader_lib TARGET_NAME)
  set(oneValueArgs OUTPUT)
  set(multiValueArgs SOURCES DEPENDS)
  cmake_parse_arguments(SPIRV_SHADER_LIB "" "${oneValueArgs}" "${multiValueArgs}" ${ARGN} )

  if (IS_ABSOLUTE(SPIRV_SHADER_LIB_OUTPUT))
    set(OUTPUT_PATH "${SPIRV_SHADER_LIB_OUTPUT}")
  else()
    set(OUTPUT_PATH "${PROJECT_BINARY_DIR}/${SPIRV_SHADER_LIB_OUTPUT}")
  endif()


  set(SPIRV_FILES "")
  foreach (GLSL_FILE ${SPIRV_SHADER_LIB_SOURCES})
    get_filename_component(FILE_NAME ${GLSL_FILE} NAME)
    set(SPIRV_FILE "${OUTPUT_PATH}/${FILE_NAME}.spv")
    add_spirv_shader(
      ${GLSL_FILE}
      ${SPIRV_FILE}
    )
    list(APPEND SPIRV_FILES ${SPIRV_FILE})
  endforeach()

  add_custom_target(${TARGET_NAME}
    SOURCES ${SPIRV_SHADER_LIB_SOURCES}
    DEPENDS ${SPIRV_FILES} ${SPIRV_SHADER_LIB_DEPENDS}
  )
endmacro()
