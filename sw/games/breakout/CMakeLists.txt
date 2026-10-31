# Breakout (SR-GAM-07 to SR-GAM-14, SR-GAM-19 to SR-GAM-22). Issue #19.
add_library(arcade_breakout breakout.cpp)
target_include_directories(arcade_breakout PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(arcade_breakout PUBLIC arcade_api PRIVATE arcade_warnings)
