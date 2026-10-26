# Each test is its own executable, registered with CTest under the file's base name.
set(LODE_TESTS
    version_test
    number_test
    canonical_test
    print_test
    property_test
    functions_test
    differentiate_test
    substitute_test
    evaluate_test
)

foreach (test_name IN LISTS LODE_TESTS)
    add_executable(${test_name} ${test_name}.cpp)
    target_link_libraries(${test_name} PRIVATE lode::lode)
    target_include_directories(${test_name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${test_name} PRIVATE -Wall -Wextra -Werror)
    endif()
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()
