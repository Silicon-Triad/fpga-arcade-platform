# Game Engine (SR-FW, SR-UI). Game loop and tick timing: issue #7. High-score store: issue #11.
add_library(arcade_engine
    game_loop.cpp
    tick_clock.cpp
    tick_profiler.cpp
    file_system.cpp
    high_score_store.cpp)
target_include_directories(arcade_engine PUBLIC ${PROJECT_SOURCE_DIR}/sw)
target_link_libraries(arcade_engine PUBLIC arcade_api PRIVATE arcade_warnings)
