cmake_minimum_required(VERSION 3.16)
project(AutoEvalTests)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find packages
find_package(spdlog CONFIG)
find_package(yaml-cpp CONFIG)
find_package(nlohmann_json CONFIG)
find_package(fmt CONFIG)

# Include dirs
include_directories(${CMAKE_SOURCE_DIR}/include)

# Helper: create test executable
function(add_autoeval_test name)
    add_executable(${name} ${ARGN})
    target_link_libraries(${name} PRIVATE
        $<TARGET_FILE:spdlog::spdlog>
        $<TARGET_FILE:yaml-cpp::yaml-cpp>
        $<TARGET_FILE:nlohmann_json::nlohmann_json>
        $<TARGET_FILE:fmt::fmt>
    )
endfunction()

# Build all test executables
add_autoeval_test(autoeval_test_types tests/unit/test_types.cpp)
add_autoeval_test(autoeval_test_metrics tests/unit/test_metrics.cpp)
add_autoeval_test(autoeval_test_loader tests/unit/test_loader.cpp)
add_autoeval_test(autoeval_test_plugin tests/unit/test_plugin.cpp)
add_autoeval_test(autoeval_test_integration tests/integration/test_evaluator.cpp)
add_autoeval_test(autoeval_bench tests/integration/test_benchmark.cpp)
