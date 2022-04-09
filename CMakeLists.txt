cmake_minimum_required(VERSION 3.9.6)
set_property(
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTY VS_STARTUP_PROJECT
    asuna)
project(asuna)

set(PROJNAME ${CMAKE_PROJECT_NAME})
set(PROJ_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(THIRD_PARTY_DIR ${PROJ_ROOT_DIR}/${PROJNAME}/third_party)

if(NOT BASE_DIRECTORY)
  find_path(BASE_DIRECTORY
    NAMES nvpro_core/cmake/setup.cmake
    PATHS ${THIRD_PARTY_DIR}
    REQUIRED
    DOC "Directory containing nvpro_core"
    )
endif()
if(EXISTS ${BASE_DIRECTORY}/nvpro_core/cmake/setup.cmake)
  include(${BASE_DIRECTORY}/nvpro_core/cmake/setup.cmake)
  include(${BASE_DIRECTORY}/nvpro_core/cmake/utilities.cmake)
else()
  message(FATAL_ERROR "could not find base directory, please set BASE_DIRECTORY to folder containing nvpro_core")
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(ARCH "x64" CACHE STRING "CPU Architecture")
else ()
  set(ARCH "x86" CACHE STRING "CPU Architecture")
endif()
set(OUTPUT_PATH ${PROJ_ROOT_DIR}/bin_${ARCH})

_add_package_VulkanSDK()
_add_package_ImGUI()
_add_nvpro_core_lib()

project(${PROJNAME} LANGUAGES C CXX)
message(STATUS "-------------------------------")
message(STATUS "Processing Project ${PROJNAME}:")

set(CMAKE_CXX_STANDARD 17)
_add_project_definitions(${PROJNAME})

file(GLOB ENTRY
    ${PROJNAME}/main.cpp)

file(GLOB SRC_TRACER
    ${PROJNAME}/tracer/tracer.h
    ${PROJNAME}/tracer/tracer.cpp)

file(GLOB SRC_TRACER_CONTEXT
    ${PROJNAME}/tracer/context/*.h
    ${PROJNAME}/tracer/context/*.cpp)

file(GLOB SRC_TRACER_SCENE
    ${PROJNAME}/tracer/scene/*.h
    ${PROJNAME}/tracer/scene/*.cpp)

file(GLOB SRC_TRACER_RESOURCE
    ${PROJNAME}/tracer/allocres/*.h
    ${PROJNAME}/tracer/allocres/*.cpp)

file(GLOB SRC_TRACER_PIPELINE
    ${PROJNAME}/tracer/pipeline/*.h
    ${PROJNAME}/tracer/pipeline/*.cpp)

# Source files for this project
file(GLOB SRC_ASSETS_HOSTDEVICE
    ${PROJNAME}/assets/hostdevice/*.h
)

file(GLOB SRC_ASSETS_SHADERS_RAYTRACE_COMMON
    ${PROJNAME}/assets/shaders/raytrace/common/*.glsl
)

file(GLOB SRC_ASSETS_SHADERS_RAYTRACE
    ${PROJNAME}/assets/shaders/raytrace/*.rgen
    ${PROJNAME}/assets/shaders/raytrace/*.rahit
    ${PROJNAME}/assets/shaders/raytrace/*.rchit
    ${PROJNAME}/assets/shaders/raytrace/*.rmiss
)

file(GLOB SRC_ASSETS_SHADERS_GRAPHIC
    ${PROJNAME}/assets/shaders/graphic/*.frag
    ${PROJNAME}/assets/shaders/graphic/*.vert
)

file(GLOB SRC_ASSETS_SHADERS_POST
    ${PROJNAME}/assets/shaders/post/*.frag
    ${PROJNAME}/assets/shaders/post/*.vert
)

file(GLOB SRC_3RDPARTY_JSON
    ${PROJNAME}/third_party/json/json.hpp
)

source_group("asuna" FILES ${ENTRY})
source_group("asuna\\third_party\\json" FILES ${SRC_3RDPARTY_JSON})
source_group("asuna\\tracer" FILES ${SRC_TRACER})
source_group("asuna\\tracer\\context" FILES ${SRC_TRACER_CONTEXT})
source_group("asuna\\tracer\\scene" FILES ${SRC_TRACER_SCENE})
source_group("asuna\\tracer\\resource" FILES ${SRC_TRACER_RESOURCE})
source_group("asuna\\tracer\\pipeline" FILES ${SRC_TRACER_PIPELINE})
source_group("asuna\\assets\\hostdevice" FILES ${SRC_ASSETS_HOSTDEVICE})
source_group("asuna\\assets\\shaders\\raytrace" FILES ${SRC_ASSETS_SHADERS_RAYTRACE})
source_group("asuna\\assets\\shaders\\raytrace\\common" FILES ${SRC_ASSETS_SHADERS_RAYTRACE_COMMON})
source_group("asuna\\assets\\shaders\\graphic" FILES ${SRC_ASSETS_SHADERS_GRAPHIC})
source_group("asuna\\assets\\shaders\\post" FILES ${SRC_ASSETS_SHADERS_POST})

# GLSL to SPIR-V custom build
set(SAMPLES_BASESAMPLE_DEPENDENCIES OFF CACHE BOOL "Create dependies on GLSL files")
compile_glsl(
    SOURCE_FILES
        ${PROJ_ROOT_DIR}/${PROJNAME}/assets/shaders/raytrace/raytrace.uvtest.rchit
        ${PROJ_ROOT_DIR}/${PROJNAME}/assets/shaders/raytrace/raytrace.uvtest.rgen
        ${PROJ_ROOT_DIR}/${PROJNAME}/assets/shaders/raytrace/raytrace.uvtest.rmiss
    HEADER_FILES
        ${SRC_ASSETS_HOSTDEVICE}
        ${SRC_ASSETS_SHADERS_RAYTRACE_COMMON}
    DST
        "${PROJNAME}/assets/@autogen"
    VULKAN_TARGET
        "vulkan1.3"
    HEADER ON
    DEPENDENCY ON
)
compile_glsl(
    SOURCE_FILES
        ${SRC_ASSETS_SHADERS_GRAPHIC}
    HEADER_FILES
        ${SRC_ASSETS_HOSTDEVICE}
    DST
        "${PROJNAME}/assets/@autogen"
    VULKAN_TARGET
        "vulkan1.3"
    HEADER ON
    DEPENDENCY ON
)
compile_glsl(
    SOURCE_FILES
        ${SRC_ASSETS_SHADERS_POST}
    HEADER_FILES
        ${SRC_ASSETS_HOSTDEVICE}
    DST
        "${PROJNAME}/assets/@autogen"
    VULKAN_TARGET
        "vulkan1.3"
    HEADER ON
    DEPENDENCY ON
)
compile_glsl(
    SOURCE_FILES
        ${PROJ_ROOT_DIR}/${PROJNAME}/assets/shaders/raytrace/raytrace.intersectiontest.rchit
        ${PROJ_ROOT_DIR}/${PROJNAME}/assets/shaders/raytrace/raytrace.intersectiontest.rgen
        ${PROJ_ROOT_DIR}/${PROJNAME}/assets/shaders/raytrace/raytrace.intersectiontest.rmiss
    HEADER_FILES
        ${SRC_ASSETS_HOSTDEVICE}
        ${SRC_ASSETS_SHADERS_RAYTRACE_COMMON}
    DST
        "${PROJNAME}/assets/@autogen"
    VULKAN_TARGET
        "vulkan1.3"
    HEADER ON
    DEPENDENCY ON
)

add_executable(${PROJNAME}
    ${ENTRY}
    ${SRC_3RDPARTY_JSON}
    ${SRC_TRACER}
    ${SRC_TRACER_CONTEXT}
    ${SRC_TRACER_SCENE}
    ${SRC_TRACER_RESOURCE}
    ${SRC_TRACER_PIPELINE}
    ${SRC_ASSETS_HOSTDEVICE})

target_sources(${PROJNAME} PUBLIC
    ${SRC_ASSETS_SHADERS_GRAPHIC}
    ${SRC_ASSETS_SHADERS_POST}
    ${SRC_ASSETS_SHADERS_RAYTRACE_COMMON}
    ${SRC_ASSETS_SHADERS_RAYTRACE})

target_sources(${PROJNAME} PUBLIC ${COMMON_SOURCE_FILES})
target_sources(${PROJNAME} PUBLIC ${PACKAGE_SOURCE_FILES})
source_group("Common Package" FILES ${COMMON_SOURCE_FILES} ${PACKAGE_SOURCE_FILES})
target_link_libraries(${PROJNAME} ${PLATFORM_LIBRARIES} nvpro_core)

foreach(DEBUGLIB ${LIBRARIES_DEBUG})
    target_link_libraries(${PROJNAME} debug ${DEBUGLIB})
endforeach(DEBUGLIB)
foreach(RELEASELIB ${LIBRARIES_OPTIMIZED})
    target_link_libraries(${PROJNAME} optimized ${RELEASELIB})
endforeach(RELEASELIB)

# copies binaries that need to be put next to the exe files (ZLib, etc.)
_finalize_target(${PROJNAME})