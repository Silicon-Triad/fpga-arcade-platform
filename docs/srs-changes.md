# SRS changes

Changes agreed after SRS rev 1.0. `docs/srs/` itself is never edited (R-1.3).

## Game::lives() added to the game interface (#92)

SR-FW-03 lists five game functions. A sixth, optional one is added: `lives()`, which returns the lives left or nullopt for a game without lives. The engine needs it to show Breakout's lives in the HUD (SR-UI-04).

Status: approved by all three members (#92, merged in #94).

## Breakout HUD: SR-UI-04 and section 3.1.1 screen 4 disagree (#10)

SR-UI-04 asks the HUD to show the lives (Breakout only) and the game's current high score. Section 3.1.1 screen 4 shows `LIVES 3` on the right and no HI. Release code follows screen 4, so Breakout's HUD shows no high score. The Game Over box shows HI for both games.

Status: open; where to show Breakout's high score is still to be decided.
