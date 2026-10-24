# C++ program and unit tests. Modules add themselves here as their issues land.

# Warning flags shared by every target (R-4.1.1).
add_library(arcade_warnings INTERFACE)
target_compile_options(arcade_warnings INTERFACE -Wall -Wextra -Wpedantic -Wshadow -Wconversion)
if(ARCADE_WERROR)
    target_compile_options(arcade_warnings INTERFACE -Werror)
endif()

# Frozen public headers: Arcade API, engine API, game interface, shared types (issue #3).
add_library(arcade_api INTERFACE)
target_include_directories(arcade_api INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

if(ARCADE_COVERAGE)
    add_compile_options(--coverage -O0)
    add_link_options(--coverage)
endif()

add_subdirectory(input)
add_subdirectory(hal)
add_subdirectory(tools)

if(ARCADE_BUILD_TESTS)
    # GoogleTest: use the system package (libgtest-dev) if present, else fetch it.
    find_package(GTest 1.14 QUIET)
    if(NOT GTest_FOUND)
        include(FetchContent)
        FetchContent_Declare(googletest
            URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
        set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(googletest)  # defines GTest::gtest_main itself
    endif()
    include(GoogleTest)
    add_subdirectory(tests)
endif()

if(ARCADE_COVERAGE)
    find_program(GCOVR gcovr REQUIRED)
    # Report only; the 80 % gate on sw/engine and sw/games is issue #24 (QA-04).
    add_custom_target(coverage
        COMMAND ${CMAKE_COMMAND} -E make_directory ${PROJECT_BINARY_DIR}/coverage
        COMMAND ${GCOVR} --root ${PROJECT_SOURCE_DIR} --filter ${PROJECT_SOURCE_DIR}/sw/
                --exclude ${PROJECT_SOURCE_DIR}/sw/tests/ --print-summary
                --html-details ${PROJECT_BINARY_DIR}/coverage/index.html
                ${PROJECT_BINARY_DIR}
        COMMAND ${CMAKE_COMMAND} -E echo "Coverage report: ${PROJECT_BINARY_DIR}/coverage/index.html"
        COMMENT "Collecting line coverage")
endif()
