# SRS changes

Changes agreed after SRS rev 1.0. `docs/srs/` itself is never edited (R-1.3).

## Game::lives() added to the game interface (#92)

SR-FW-03 lists five game functions. A sixth, optional one is added: `lives()`, which returns the lives left or nullopt for a game without lives. The engine needs it to show Breakout's lives in the HUD (SR-UI-04).

Status: approved by all three members (#92, merged in #94).
