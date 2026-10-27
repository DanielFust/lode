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
    function_test
    parameters_test
    tensor_test
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

# Code generation: a generator program writes C++ code for the test functions during the build,
# and codegen_test compiles that code and checks it against the interpreter
add_executable(codegen_generator codegen_generator.cpp)
target_link_libraries(codegen_generator PRIVATE lode::lode)
target_include_directories(codegen_generator PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

set(LODE_GENERATED_CASES ${CMAKE_CURRENT_BINARY_DIR}/generated/generated_cases.hpp)
add_custom_command(
    OUTPUT ${LODE_GENERATED_CASES}
    COMMAND codegen_generator ${LODE_GENERATED_CASES}
    DEPENDS codegen_generator
    COMMENT "Generating C++ code for the code generation test")

add_executable(codegen_test codegen_test.cpp ${LODE_GENERATED_CASES})
target_link_libraries(codegen_test PRIVATE lode::lode)
target_include_directories(codegen_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
                                                ${CMAKE_CURRENT_BINARY_DIR}/generated)
add_test(NAME codegen_test COMMAND codegen_test)

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(codegen_generator PRIVATE -Wall -Wextra -Werror)
    target_compile_options(codegen_test PRIVATE -Wall -Wextra -Werror)
endif()
