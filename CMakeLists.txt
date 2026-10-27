cmake_minimum_required(VERSION 3.20)
project(lode VERSION 0.1.0 LANGUAGES CXX)

# LODE is header-only: an interface target carries the include path and language level.
add_library(lode INTERFACE)
add_library(lode::lode ALIAS lode)
target_include_directories(lode INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_compile_features(lode INTERFACE cxx_std_20)

# Tests build by default only when LODE is the top-level project.
option(LODE_BUILD_TESTS "Build the LODE tests" ${PROJECT_IS_TOP_LEVEL})
if (LODE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Example programs build by default when LODE is the top-level project, and run as tests so
# that they keep working. examples/01_expressions.cpp builds build/examples/01_expressions.
option(LODE_BUILD_EXAMPLES "Build the LODE example programs" ${PROJECT_IS_TOP_LEVEL})
if (LODE_BUILD_EXAMPLES)
    enable_testing()
    file(GLOB lode_examples CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/examples/*.cpp")
    foreach (source IN LISTS lode_examples)
        get_filename_component(name "${source}" NAME_WE)
        add_executable(lode_example_${name} "${source}")
        set_target_properties(lode_example_${name} PROPERTIES
            OUTPUT_NAME "${name}"
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/examples")
        target_link_libraries(lode_example_${name} PRIVATE lode::lode)
        if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(lode_example_${name} PRIVATE -Wall -Wextra -Werror)
        endif()
        add_test(NAME example_${name} COMMAND lode_example_${name})
    endforeach()
endif()

# Benchmarks build on request; time them in a Release build.
option(LODE_BUILD_BENCHMARKS "Build the LODE benchmarks" OFF)
if (LODE_BUILD_BENCHMARKS)
    add_executable(lode_benchmark bench/benchmark.cpp)
    target_link_libraries(lode_benchmark PRIVATE lode::lode)
    if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(lode_benchmark PRIVATE -Wall -Wextra -Werror)
    endif()
endif()
