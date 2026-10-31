# Game Engine (SR-FW, SR-UI). Game loop and tick timing: issue #7.
add_library(arcade_engine
    game_loop.cpp
    hint_text.cpp
    tick_clock.cpp
    tick_profiler.cpp
    ui_layout.cpp)
target_include_directories(arcade_engine PUBLIC ${PROJECT_SOURCE_DIR}/sw)
target_link_libraries(arcade_engine PUBLIC arcade_api arcade_assets PRIVATE arcade_warnings)
