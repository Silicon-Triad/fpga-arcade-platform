# One subdirectory per module (R-4.1.11).

# arcade_add_test(<name> SOURCES <files...> [LIBS <targets...>])
function(arcade_add_test name)
    cmake_parse_arguments(ARG "" "" "SOURCES;LIBS" ${ARGN})
    add_executable(${name} ${ARG_SOURCES})
    target_link_libraries(${name} PRIVATE GTest::gtest_main arcade_warnings ${ARG_LIBS})
    gtest_discover_tests(${name})
endfunction()

add_subdirectory(arcade_api)
add_subdirectory(smoke)
