# Font, mosaic and common tiles, palette and text layout (SR-FW-12). Issue #9.
add_library(arcade_assets
    font.cpp
    game_tiles.cpp
    text_tiles.cpp
    tile_set.cpp)
target_include_directories(arcade_assets PUBLIC ${PROJECT_SOURCE_DIR}/sw)
target_link_libraries(arcade_assets PUBLIC arcade_api PRIVATE arcade_warnings)
