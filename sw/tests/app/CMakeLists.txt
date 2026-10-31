arcade_add_test(startup_test SOURCES startup_test.cpp LIBS arcade_startup arcade_engine arcade_assets)
if(ARCADE_BACKEND_SDL)
    arcade_add_test(binding_lookup_test SOURCES binding_lookup_test.cpp LIBS arcade_input)
endif()
