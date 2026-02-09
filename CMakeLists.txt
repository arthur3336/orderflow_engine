# Minimum CMake version required - 3.16 has good C++17 support
cmake_minimum_required(VERSION 3.16)

# Project name and language
# This creates variables like PROJECT_NAME, PROJECT_SOURCE_DIR
project(orderbook LANGUAGES CXX)

# Set C++ standard to C++17
# C++17 gives us: std::optional, structured bindings, if-init statements
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)  # Fail if compiler doesn't support C++17

# Add compiler warnings - helps catch bugs early
# These flags work with GCC and Clang
add_compile_options(-Wall -Wextra -Wpedantic)

# Create the main executable
add_executable(orderbook
    src/main.cpp
    src/OrderBook.cpp
)

# Create the simulation executable
add_executable(simulation
    src/simulation.cpp
    src/OrderBook.cpp
)

# Create the benchmark executable
add_executable(benchmark
    src/benchmark.cpp
    src/OrderBook.cpp
)

# Tell CMake where to find header files
target_include_directories(orderbook PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

target_include_directories(simulation PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

target_include_directories(benchmark PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

# C FFI wrapper library
add_subdirectory(ffi)
