# Test tools (sw/tools/README.md).
add_subdirectory(uinput)

# Manual SDL2 backend demo (issue #17, SR-FW-06). Not a test: run it and press keys.
if(ARCADE_BACKEND_SDL)
    add_executable(sdl_demo sdl_demo.cpp)
    target_link_libraries(sdl_demo PRIVATE arcade_backend_sdl arcade_warnings)
endif()
