# Hardware Access Layer (SR-HAL). register_map.hpp is FROZEN; only sw/hal/ and sw/tests/hal/
# may include it (QA-01).
add_library(arcade_hal tilemap_hal.cpp)
target_include_directories(arcade_hal PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(arcade_hal PRIVATE arcade_warnings)

# LW-bridge write throughput benchmark (#38; §3.1.4 data rate, PR-04). The logic is a library
# so sw/tests/hal/ can test it on the PC; the program needs /dev/mem and mmap, so Linux only.
add_library(arcade_bridge_bench bridge_benchmark.cpp)
target_include_directories(arcade_bridge_bench PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(arcade_bridge_bench PRIVATE arcade_warnings)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_executable(bridge_benchmark bridge_benchmark_main.cpp)
    target_link_libraries(bridge_benchmark PRIVATE arcade_bridge_bench arcade_warnings)
endif()
