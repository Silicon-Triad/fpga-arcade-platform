# The arcade program (issue #16, SR-FW-01, SR-FW-05, SF-02).
add_library(arcade_startup options.cpp startup.cpp)
target_include_directories(arcade_startup PUBLIC ${PROJECT_SOURCE_DIR}/sw)
target_link_libraries(arcade_startup
    PUBLIC arcade_api
    PRIVATE arcade_engine arcade_assets arcade_game_registry arcade_warnings)

add_executable(arcade main.cpp)
target_link_libraries(arcade PRIVATE arcade_startup arcade_warnings)
if(ARCADE_BACKEND_SDL)
    target_compile_definitions(arcade PRIVATE ARCADE_HAS_SDL)
    target_link_libraries(arcade PRIVATE arcade_backend_sdl arcade_input)
endif()
