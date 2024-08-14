cmake_minimum_required(VERSION 3.5.0)
project(Bulkan VERSION 0.1.0 LANGUAGES C CXX)

set(TRIPLET "x64-windows")

# Initialize vcpkg
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/vcpkg_installed")
    message(STATUS "vcpkg_installed directory not found. Running vcpkg install...")
    execute_process(
        COMMAND vcpkg install
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        RESULT_VARIABLE VCPKG_INSTALL_RESULT
    )
endif()
set(CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/vcpkg_installed/${TRIPLET}/share")

# Find Packages
find_package(glfw3 CONFIG REQUIRED)
find_package(Vulkan REQUIRED)

add_executable(Bulkan src/main.cpp)

# Link Libraries
target_link_libraries(Bulkan PRIVATE glfw)
target_link_libraries(Bulkan PRIVATE Vulkan::Vulkan)

# Copy DLLs
set(PATH_TO_DLLS "${CMAKE_SOURCE_DIR}/vcpkg_installed/${TRIPLET}/bin")
add_custom_command(
    TARGET Bulkan POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${PATH_TO_DLLS}"
    $<TARGET_FILE_DIR:Bulkan>
)

# Compile Shaders
set(SHADER_DIR "${CMAKE_SOURCE_DIR}/src/shaders")
set(SHADER_BIN_DIR "${CMAKE_BINARY_DIR}/shaders")
set(GLSLC "C:/VulkanSDK/1.3.275.0/Bin/glslc.exe")
set(SHADER_FRAG "${SHADER_DIR}/shader.frag")
set(SPIRV_FRAG "${SHADER_BIN_DIR}/frag.spv")
add_custom_command(
    OUTPUT ${SPIRV_FRAG}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_BIN_DIR}
    COMMAND ${GLSLC} ${SHADER_FRAG} -o ${SPIRV_FRAG}
    DEPENDS ${SHADER_FRAG}
    COMMENT "Compiling shader.frag..."
)
set(SHADER_VERT "${SHADER_DIR}/shader.vert")
set(SPIRV_VERT "${SHADER_BIN_DIR}/vert.spv")
add_custom_command(
    OUTPUT ${SPIRV_VERT}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_BIN_DIR}
    COMMAND ${GLSLC} ${SHADER_VERT} -o ${SPIRV_VERT}
    DEPENDS ${SHADER_VERT}
    COMMENT "Compiling shader.vert..."
)
add_custom_target(
    Shaders
    DEPENDS ${SPIRV_FRAG} ${SPIRV_VERT}
)
add_dependencies(Bulkan Shaders)

# Enable Testing
include(CTest)
enable_testing()

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)