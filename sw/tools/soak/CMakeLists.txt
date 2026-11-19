# Soak driver (QA-05, issue #51). Linux only: it uses the uinput tools of #23.
add_library(arcade_soak_plan soak_plan.cpp)
target_include_directories(arcade_soak_plan PUBLIC ${PROJECT_SOURCE_DIR}/sw)
target_link_libraries(arcade_soak_plan PRIVATE arcade_warnings)

add_executable(soak_driver soak_main.cpp)
target_link_libraries(soak_driver PRIVATE arcade_soak_plan arcade_uinput arcade_warnings)
