cmake_minimum_required(VERSION 3.16)
project(
  V4Link
  VERSION 0.1.0
  LANGUAGES CXX)

# ============================================================================
# Project Configuration
# ============================================================================

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build options
option(V4LINK_BUILD_TESTS "Build unit tests" ON)
option(V4LINK_OPTIMIZE_SIZE "Optimize for size (-Os)" ON)
option(V4LINK_ENABLE_LTO "Enable Link Time Optimization" OFF)

# Dependency paths
set(V4_LOCAL_PATH
    "${CMAKE_CURRENT_SOURCE_DIR}/../V4"
    CACHE PATH "Path to local V4 source")

# ============================================================================
# V4 VM Dependency
# ============================================================================

include(FetchContent)

if(EXISTS "${V4_LOCAL_PATH}/CMakeLists.txt")
  message(STATUS "Using local V4 from: ${V4_LOCAL_PATH}")
  add_subdirectory(${V4_LOCAL_PATH} ${CMAKE_BINARY_DIR}/v4)
else()
  message(FATAL_ERROR "V4 not found at ${V4_LOCAL_PATH}. Please clone V4 VM repository.")
endif()

# ============================================================================
# V4-link Library
# ============================================================================

set(V4LINK_SOURCES src/link.cpp src/link_c_api.cpp src/frame.cpp src/crc8.cpp)

add_library(v4link STATIC ${V4LINK_SOURCES})

target_include_directories(
  v4link
  PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include> $<INSTALL_INTERFACE:include>
  PRIVATE ${PROJECT_SOURCE_DIR}/src)

# Link V4 VM library Note: v4vm requires mock_hal for testing/development builds
if(TARGET V4::mock_hal)
  target_link_libraries(v4link PUBLIC v4vm V4::mock_hal)
else()
  target_link_libraries(v4link PUBLIC v4vm)
endif()

# Compiler flags
if(MSVC)
  target_compile_options(
    v4link
    PRIVATE /W4 # Warning level 4
            /GR- # Disable RTTI
            /EHs-c- # Disable exceptions
  )
  target_compile_options(v4link PRIVATE $<$<CONFIG:Debug>:/Od> $<$<CONFIG:Release>:/O1>)
else()
  target_compile_options(
    v4link
    PRIVATE -Wall # Enable all warnings
            -Wextra # Enable extra warnings
            -fno-exceptions -fno-rtti)
  if(V4LINK_OPTIMIZE_SIZE)
    target_compile_options(v4link PRIVATE -Os)
  endif()
endif()

# Enable LTO if supported and requested
if(V4LINK_ENABLE_LTO)
  include(CheckIPOSupported)
  check_ipo_supported(
    RESULT ipo_supported
    OUTPUT ipo_output
    LANGUAGES CXX)
  if(ipo_supported)
    set_target_properties(v4link PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
    message(STATUS "LTO enabled for v4link")
  else()
    message(WARNING "LTO not supported: ${ipo_output}")
  endif()
endif()

# ============================================================================
# Tests
# ============================================================================

if(V4LINK_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

# ============================================================================
# Installation (optional)
# ============================================================================

install(
  TARGETS v4link
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin)

install(DIRECTORY include/v4link DESTINATION include/v4link)

# ============================================================================
# Status Summary
# ============================================================================

message(STATUS "")
message(STATUS "V4-link Configuration:")
message(STATUS "  Version:       ${PROJECT_VERSION}")
message(STATUS "  Build tests:   ${V4LINK_BUILD_TESTS}")
message(STATUS "  Optimize size: ${V4LINK_OPTIMIZE_SIZE}")
message(STATUS "  Enable LTO:    ${V4LINK_ENABLE_LTO}")
message(STATUS "  V4 path:       ${V4_LOCAL_PATH}")
message(STATUS "")
