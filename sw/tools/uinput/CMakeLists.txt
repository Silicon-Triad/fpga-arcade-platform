# uinput test tools (PR-06, issue #23). Linux only.
find_package(Threads REQUIRED)

add_library(arcade_uinput
    inject_plan.cpp
    options.cpp
    throughput_check.cpp
    uinput_device.cpp)
target_include_directories(arcade_uinput PUBLIC ${PROJECT_SOURCE_DIR}/sw)
target_link_libraries(arcade_uinput PRIVATE arcade_warnings)

add_executable(uinput_inject uinput_inject_main.cpp)
target_link_libraries(uinput_inject PRIVATE arcade_uinput arcade_input arcade_warnings)

add_executable(input_throughput_test input_throughput_main.cpp)
target_link_libraries(input_throughput_test
    PRIVATE arcade_uinput arcade_input arcade_warnings Threads::Threads)
