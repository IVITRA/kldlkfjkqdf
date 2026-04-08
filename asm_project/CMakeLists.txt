cmake_minimum_required(VERSION 3.16)
project(ASM_Core VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Optimization flags
if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /O2 /arch:AVX2 /openmp")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -march=native -fopenmp")
endif()

# Required Libraries
find_package(Eigen3 3.4 QUIET)
find_package(Threads REQUIRED)
find_package(OpenMP QUIET)

# External dependencies via FetchContent
include(FetchContent)

# mio (Memory-mapped I/O)
FetchContent_Declare(
  mio
  GIT_REPOSITORY https://github.com/mandreyel/mio.git
  GIT_TAG master
)
FetchContent_MakeAvailable(mio)

# spdlog (Logging)
FetchContent_Declare(
  spdlog
  GIT_REPOSITORY https://github.com/gabime/spdlog.git
  GIT_TAG v1.12.0
)
FetchContent_MakeAvailable(spdlog)

# nlohmann/json (Config)
FetchContent_Declare(
  json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.2
)
FetchContent_MakeAvailable(json)

# Google Test (Testing)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.14.0
)
FetchContent_MakeAvailable(googletest)

# Main executable
add_executable(asm_core 
    src/main.cpp
    src/memory_manager.cpp
    src/router.cpp
    src/async_loader.cpp
    src/context_protocol.cpp
    src/inference_engine.cpp
)

# Include directories
target_include_directories(asm_core PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/formats
)

# Link libraries
target_link_libraries(asm_core 
    Eigen3::Eigen
    mio::mio
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    OpenMP::OpenMP_CXX
    Threads::Threads
)

# Tests
enable_testing()

add_executable(asm_tests
    tests/test_memory_manager.cpp
    tests/test_router.cpp
    tests/test_context.cpp
    tests/test_critical_checks.cpp
    tests/test_asm_benchmark.cpp
)

target_include_directories(asm_tests PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/formats
)

target_link_libraries(asm_tests
    gtest
    gtest_main
    Eigen3::Eigen
    mio::mio
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    Threads::Threads
)

add_test(NAME asm_tests COMMAND asm_tests)

# Benchmarks (optional, requires google benchmark)
option(BUILD_BENCHMARKS "Build benchmarks" OFF)
if(BUILD_BENCHMARKS)
    FetchContent_Declare(
      benchmark
      GIT_REPOSITORY https://github.com/google/benchmark.git
      GIT_TAG v1.8.0
    )
    FetchContent_MakeAvailable(benchmark)
    
    add_executable(asm_benchmarks
        tests/benchmarks.cpp
    )
    
    target_link_libraries(asm_benchmarks
        benchmark::benchmark
        asm_core
    )
endif()
