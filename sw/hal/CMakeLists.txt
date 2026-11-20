# Hardware Access Layer (SR-HAL). register_map.hpp is FROZEN; only sw/hal/ and sw/tests/hal/
# may include it (QA-01).
add_library(arcade_hal tilemap_hal.cpp hal_lifecycle.cpp dev_mem_register_space.cpp
    linux_bridge_sys.cpp tone_sequencer.cpp codec_config.cpp linux_codec_sys.cpp)
target_include_directories(arcade_hal PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
# arcade_api: TBL-SoundEffect and EffectId for the sound sequencer (#44, SR-HAL-11).
target_link_libraries(arcade_hal PUBLIC arcade_api PRIVATE arcade_warnings)

# LW-bridge write throughput benchmark (#38; §3.1.4 data rate, PR-04). The logic is a library
# so sw/tests/hal/ can test it on the PC; the program needs /dev/mem and mmap, so Linux only.
add_library(arcade_bridge_bench bridge_benchmark.cpp)
target_include_directories(arcade_bridge_bench PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(arcade_bridge_bench PRIVATE arcade_warnings)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_executable(bridge_benchmark bridge_benchmark_main.cpp)
    target_link_libraries(bridge_benchmark PRIVATE arcade_bridge_bench arcade_warnings)
endif()

# Codec set-up check on the board (#43, SR-HAL-10): runs configureCodec() and prints each step.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_executable(codec_setup codec_setup_main.cpp)
    target_link_libraries(codec_setup PRIVATE arcade_hal arcade_warnings)
endif()
