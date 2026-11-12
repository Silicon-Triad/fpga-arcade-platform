# FPGA backend of the Arcade API (SR-FW-05, QA-03). Needs no SDL2, so it is built for every
# preset: the board build links it into arcade, and the PC builds run its unit tests.
add_library(arcade_backend_fpga fpga_backend.cpp)
target_include_directories(arcade_backend_fpga PUBLIC ${PROJECT_SOURCE_DIR}/sw)
target_link_libraries(arcade_backend_fpga
    PUBLIC arcade_api arcade_hal arcade_input
    PRIVATE arcade_assets arcade_warnings)
