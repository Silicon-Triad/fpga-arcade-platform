arcade_add_test(game_loop_test SOURCES game_loop_test.cpp LIBS arcade_engine)
arcade_add_test(ui_layout_test SOURCES ui_layout_test.cpp LIBS arcade_engine)
arcade_add_test(menu_test SOURCES menu_test.cpp LIBS arcade_engine)
arcade_add_test(game_registry_test SOURCES game_registry_test.cpp
    LIBS arcade_game_registry arcade_assets arcade_breakout)
