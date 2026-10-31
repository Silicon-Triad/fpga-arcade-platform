# Snake (SR-GAM-01 to SR-GAM-06, SR-GAM-17, SR-GAM-18). Issue #18.
add_library(arcade_snake snake_game.cpp)
target_include_directories(arcade_snake PUBLIC ${PROJECT_SOURCE_DIR}/sw)
target_link_libraries(arcade_snake PUBLIC arcade_api PRIVATE arcade_warnings)
