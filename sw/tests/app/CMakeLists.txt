arcade_add_test(startup_test SOURCES startup_test.cpp LIBS arcade_startup arcade_engine arcade_assets)
arcade_add_test(app_shutdown_test SOURCES shutdown_test.cpp
    LIBS arcade_startup arcade_engine arcade_assets)
if(ARCADE_BACKEND_SDL)
    target_compile_definitions(app_shutdown_test PRIVATE ARCADE_HAS_SDL)
    target_link_libraries(app_shutdown_test PRIVATE arcade_backend_sdl)
    arcade_add_test(binding_lookup_test SOURCES binding_lookup_test.cpp LIBS arcade_input)
endif()
