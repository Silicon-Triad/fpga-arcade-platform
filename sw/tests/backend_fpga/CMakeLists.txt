arcade_add_test(fpga_backend_test SOURCES fpga_backend_test.cpp
    LIBS arcade_backend_fpga arcade_assets)
if(ARCADE_BACKEND_SDL)
    # SR-FW-05: the same calls on SdlBackend, to prove both backends agree.
    target_compile_definitions(fpga_backend_test PRIVATE ARCADE_HAS_SDL)
    target_link_libraries(fpga_backend_test PRIVATE arcade_backend_sdl)
endif()
