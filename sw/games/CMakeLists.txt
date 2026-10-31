# Games. Each includes only arcade/*.hpp and the standard library (QA-01, QA-03).
add_subdirectory(snake)
add_subdirectory(breakout)

# TBL-GameRegistry: adding a game adds one row here, no engine change (SR-FW-04, QA-02).
add_library(arcade_game_registry game_registry.cpp)
target_include_directories(arcade_game_registry PUBLIC ${PROJECT_SOURCE_DIR}/sw)
target_link_libraries(arcade_game_registry
    PUBLIC arcade_api
    PRIVATE arcade_assets arcade_snake arcade_breakout arcade_warnings)
