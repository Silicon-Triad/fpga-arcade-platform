# SF-04 power-cut test program (issue #45).
add_library(arcade_save_loop save_loop.cpp)
target_include_directories(arcade_save_loop PUBLIC ${PROJECT_SOURCE_DIR}/sw)
target_link_libraries(arcade_save_loop PUBLIC arcade_engine PRIVATE arcade_warnings)

add_executable(save_loop save_loop_main.cpp)
target_link_libraries(save_loop PRIVATE arcade_save_loop arcade_warnings)
