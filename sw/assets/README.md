# `sw/assets/`

**Owner:** Muhammad  
**Issues:** #9

Font, quadrant-mosaic tiles, game tiles and palette values. SRS: SR-FW-12, §3.1.1 tile budget.

- `tile_ids.hpp`: tile numbering. Font 0-94, mosaic 95-110, common tiles from 111, game tiles
  from `kFirstGameTile`.
- `font.hpp/.cpp`: 5 x 7 font, ASCII 0x20-0x7E.
- `tile_set.hpp/.cpp`: `buildTileSet()`, the one set passed to `loadTiles()`.
- `text_tiles.hpp/.cpp`: `textToTiles()`, text layout shared by the backends.
- `palette.hpp`: `kPalette`. RGB values are a design choice, not SRS values.

Rules: see [`AGENTS.md`](/AGENTS.md).
