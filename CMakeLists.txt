# Top-level build for the FPGA Arcade Platform C++ program (AGENTS.md section 3.1).
cmake_minimum_required(VERSION 3.25)  # CMakePresets.json schema v6
project(fpga_arcade VERSION 1.0.0 LANGUAGES CXX)

# CON-08: C++17, no compiler extensions (R-4.1.1).
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ARCADE_BUILD_TESTS "Build GoogleTest unit tests" ON)
option(ARCADE_WERROR "Treat compiler warnings as errors" ON)
option(ARCADE_COVERAGE "Instrument for line coverage (gcovr)" OFF)

if(ARCADE_BUILD_TESTS)
    enable_testing()
endif()

add_subdirectory(sw)
