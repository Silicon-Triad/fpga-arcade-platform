# SDL2 backend of the Arcade API, PC only (SR-FW-05 to SR-FW-07). Built only when
# ARCADE_BACKEND_SDL is ON, so the board build never needs or links SDL2 (srs-mujtaba-drop1.md
# section 3.1.3, AS-08).
find_package(SDL2 REQUIRED)

add_library(arcade_backend_sdl
    key_codes.cpp
    sdl_backend.cpp)
target_include_directories(arcade_backend_sdl PUBLIC ${PROJECT_SOURCE_DIR}/sw)
target_link_libraries(arcade_backend_sdl PUBLIC arcade_api arcade_assets SDL2::SDL2 PRIVATE arcade_warnings)
