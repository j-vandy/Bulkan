cmake_minimum_required(VERSION 3.5.0)
project(Bulkan VERSION 0.1.0 LANGUAGES C CXX)

add_executable(Bulkan main.cpp)

# COMPILE SHADERS
set(SHADER_DIR "${CMAKE_SOURCE_DIR}/shaders")
set(SHADER_BIN_DIR "${CMAKE_SOURCE_DIR}/shaders/bin")
set(GLSLC "C:/VulkanSDK/1.3.275.0/Bin/glslc.exe")

file(GLOB_RECURSE SHADER_FILES
    "shaders/*.frag"
    "shaders/*.vert"
)

foreach(GLSL ${SHADER_FILES})
    get_filename_component(FILE_NAME ${GLSL} NAME)
    set(SPIRV "${SHADER_BIN_DIR}/${FILE_NAME}.spv")
    add_custom_command(
        OUTPUT ${SPIRV}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_BIN_DIR}
        COMMAND ${GLSLC} ${GLSL} -o ${SPIRV}
        DEPENDS ${GLSL}
    )
    list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(GLSL)

add_custom_target(
    Shaders
    DEPENDS ${SPIRV_BINARY_FILES}
)

add_dependencies(Bulkan Shaders)

include(CTest)
enable_testing()

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
