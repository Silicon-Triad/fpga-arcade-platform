# Input Manager (SR-INP). Linux only: it reads evdev devices under /dev/input.
add_library(arcade_input
    input_manager.cpp
    linux_evdev_sys.cpp)
target_include_directories(arcade_input PUBLIC ${PROJECT_SOURCE_DIR}/sw)
target_link_libraries(arcade_input PRIVATE arcade_warnings)
