# `sw/games/breakout/`

**Owner:** Muhammad  
**Issues:** #19

Breakout. Includes only `arcade/*.hpp` and the standard library (QA-01). SRS: SR-GAM-07 to 14, 19 to 22.

Rules: see [`AGENTS.md`](/AGENTS.md).

`Breakout(firstTile, blankTile)`: the engine passes the index of its first game tile and the
blank tile, because games cannot include `sw/assets/` (QA-01). Tiles are drawn as
`firstTile + BreakoutTile`. HUD text (row 1) is drawn by the engine (SR-UI-04, #10).
Tests: `sw/tests/games/breakout_test.cpp`.
