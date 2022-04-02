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
    ${PROJNAME}/main.cpp
)

source_group("asuna" FILES ${ENTRY})

add_executable(${PROJNAME}
    ${ENTRY})

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