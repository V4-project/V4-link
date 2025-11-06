# ============================================================================
# V4-link Tests
# ============================================================================

# Fetch doctest (using URL to avoid CMakeLists.txt issues)
include(FetchContent)
fetchcontent_declare(
  doctest
  URL https://github.com/doctest/doctest/releases/download/v2.4.11/doctest.h
  DOWNLOAD_NO_EXTRACT TRUE)
fetchcontent_makeavailable(doctest)

# Create interface library for doctest
add_library(doctest INTERFACE)
target_include_directories(doctest INTERFACE ${doctest_SOURCE_DIR})

# Test executable
add_executable(v4link_test test_link.cpp)

# Link libraries in correct order Static library link order matters: dependencies must
# come AFTER dependents v4link_test depends on: v4link → v4engine → mock_hal So link order
# should be: v4link, doctest, v4engine (again), mock_hal
target_link_libraries(v4link_test PRIVATE v4link doctest v4engine mock_hal)

# Include internal headers for testing
target_include_directories(v4link_test PRIVATE ${PROJECT_SOURCE_DIR}/src)

# Disable exceptions for consistency with main library (but doctest requires exceptions,
# so we need to enable them for tests only)
if(NOT MSVC)
  # Override the -fno-exceptions flag for tests
  target_compile_options(v4link_test PRIVATE -fexceptions)
endif()

# Register test with CTest
add_test(NAME v4link_test COMMAND v4link_test)

# ============================================================================
# Relocation unit tests
# ============================================================================

add_executable(v4link_relocation_test test_relocation.cpp)

# Link all required libraries (same as v4link_test)
target_link_libraries(v4link_relocation_test PRIVATE v4link doctest v4engine mock_hal)

# Include internal headers for testing
target_include_directories(v4link_relocation_test PRIVATE ${PROJECT_SOURCE_DIR}/src)

# Enable exceptions for doctest
if(NOT MSVC)
  target_compile_options(v4link_relocation_test PRIVATE -fexceptions)
endif()

# Register test with CTest
add_test(NAME v4link_relocation_test COMMAND v4link_relocation_test)

# ============================================================================
# Minimal binary for size measurement
# ============================================================================

# This minimal executable contains only the essential V4-link code without test
# frameworks, allowing us to measure the actual minimum binary size for embedded systems.
add_executable(v4link_minimal minimal_binary.cpp)

# Link only the required libraries (no doctest)
target_link_libraries(v4link_minimal PRIVATE v4link v4engine mock_hal)

# Include internal headers
target_include_directories(v4link_minimal PRIVATE ${PROJECT_SOURCE_DIR}/src)

# Apply same compilation flags as main library
if(NOT MSVC)
  target_compile_options(v4link_minimal PRIVATE -fexceptions)
endif()

# Enable LTO for minimal binary if requested
if(V4LINK_ENABLE_LTO)
  include(CheckIPOSupported)
  check_ipo_supported(
    RESULT ipo_supported
    OUTPUT ipo_output
    LANGUAGES CXX)
  if(ipo_supported)
    set_target_properties(v4link_minimal PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
  endif()
endif()
