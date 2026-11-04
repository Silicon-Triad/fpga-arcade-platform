# Hardware Access Layer (SR-HAL). register_map.hpp is FROZEN; only sw/hal/ and sw/tests/hal/
# may include it (QA-01).
add_library(arcade_hal tilemap_hal.cpp)
target_include_directories(arcade_hal PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(arcade_hal PRIVATE arcade_warnings)
