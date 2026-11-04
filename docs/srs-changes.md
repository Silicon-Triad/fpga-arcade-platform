# SRS changes

Changes agreed after SRS rev 1.0. `docs/srs/` itself is never edited (R-1.3).

## Game::lives() added to the game interface (#92)

SR-FW-03 lists five game functions. A sixth, optional one is added: `lives()`, which returns the lives left or nullopt for a game without lives. The engine needs it to show Breakout's lives in the HUD (SR-UI-04).

Status: approved by all three members (#92, merged in #94).

## Breakout HUD: SR-UI-04 and section 3.1.1 screen 4 disagree (#10)

SR-UI-04 asks the HUD to show the lives (Breakout only) and the game's current high score. Section 3.1.1 screen 4 shows `LIVES 3` on the right and no HI. Release code follows screen 4, so Breakout's HUD shows no high score. The Game Over box shows HI for both games.

Status: open; where to show Breakout's high score is still to be decided.
## "No keyboard" notice layout (#13, PR #100)

SR-UI-15 names a "no keyboard" notice but gives no text or position, so this is a design choice, not a requirement change.

- **Text and position:** `NO KEYBOARD - PLUG IN A USB KEYBOARD`, scale 1, centred on row 90.
- **Why row 90:** it is free on the menu, initials and high-score screens. On Game Over it overlays the frozen picture; in Breakout that is the paddle row (section 3.1.1 screen 4).
- **Extra state:** the notice also shows on ShutdownConfirm, beyond the four states SR-UI-15 lists, because the menu stays under that box.

## Text colour: white-only text for release 1.0 (#81)

Section 3.1.1 shows yellow, red and green text, a red shadow under the logo, and coloured bands behind text (menu highlight, hint band). The frozen `drawText` (SR-FW-12) takes no colour, so all text is drawn in white (palette entry 1) on black.

- **Decision:** option B on #81. Accept white-only text for release 1.0; `drawText` is not changed.
- **Why:** the screens read well in white, and coloured text would need a copy of the 95-tile font per colour, out of the 145 tiles left for games (section 3.1.1 tile budget).
- **Effect on verification:** the section 3.1.1 text colours, logo shadow and text bands are accepted deviations when the screens are inspected against the mockups.

Status: decided by the area owner (Muhammad) on #81. No frozen interface changes.

## Palette RGB values (#83)

TBL-Palette has 16 entries packed as `0x00RRGGBB`. The SRS fixes only entry 0 = black (section 3.1.1 screen 10b). The other values are a design choice made by the area owner (issues #9 and #75), copied here from `kPalette` in `sw/assets/palette.hpp`.

| Index | Name | 0x00RRGGBB | Source |
|---|---|---|---|
| 0 | Black | 0x000000 | SRS, section 3.1.1 screen 10b |
| 1 | White | 0xFFFFFF | design choice (all text, see #81) |
| 2 | Red | 0xFF0000 | design choice |
| 3 | Green | 0x00FF00 | design choice |
| 4 | DarkGreen | 0x008000 | design choice |
| 5 | Blue | 0x0000FF | design choice |
| 6 | Yellow | 0xFFFF00 | design choice |
| 7 | Orange | 0xFF8000 | design choice |
| 8 | Cyan | 0x00FFFF | design choice |
| 9 | Grey | 0x808080 | design choice |
| 10 | DarkGrey | 0x404040 | design choice |
| 11 | (unused) | 0x000000 | spare |
| 12 | (unused) | 0x000000 | spare |
| 13 | (unused) | 0x000000 | spare |
| 14 | (unused) | 0x000000 | spare |
| 15 | (unused) | 0x000000 | spare |

Status: agreed on #75; recorded 4 Nov 2026. If `kPalette` changes, update this table in the same PR.
