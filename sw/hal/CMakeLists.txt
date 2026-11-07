# Hardware Access Layer (SR-HAL). register_map.hpp is FROZEN; only sw/hal/ and sw/tests/hal/
# may include it (QA-01).
add_library(arcade_hal tilemap_hal.cpp bridge_benchmark.cpp)
target_include_directories(arcade_hal PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(arcade_hal PRIVATE arcade_warnings)

# LW-bridge write throughput benchmark, run on the board (issue #38, §3.1.4, PR-04).
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_executable(bridge_benchmark bridge_benchmark_main.cpp)
    target_link_libraries(bridge_benchmark PRIVATE arcade_hal arcade_warnings)
endif()
