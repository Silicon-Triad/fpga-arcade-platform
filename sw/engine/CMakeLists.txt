# Game Engine (SR-FW, SR-UI). Game loop and tick timing: issue #7.
add_library(arcade_engine
    app.cpp
    game_loop.cpp
    game_menu.cpp
    screens/playing_screen.cpp
    screens/initials_screen.cpp
    screens/game_over_screen.cpp
    screens/keyboard_lost_screen.cpp
    screens/paused_screen.cpp
    screens/shutting_down_screen.cpp
    screens/shutdown_confirm_screen.cpp
    screens/high_scores_screen.cpp
    screens/hardware_not_ready_screen.cpp
    screens/boot_screen.cpp
    hint_text.cpp
    preview_api.cpp
    tick_clock.cpp
    tick_profiler.cpp
    ui_layout.cpp)
target_include_directories(arcade_engine PUBLIC ${PROJECT_SOURCE_DIR}/sw)
target_link_libraries(arcade_engine PUBLIC arcade_api arcade_assets PRIVATE arcade_warnings)
