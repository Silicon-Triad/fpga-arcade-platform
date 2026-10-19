## 5. Other Requirements — Database Tables (Hardware)

*Note: these hardware "tables" are a documentation convention for describing fixed-layout FPGA block RAM (M10K) contents, not literal relational tables implemented by a database engine. Software tables (TBL-HighScore, TBL-GameRegistry, TBL-KeyBinding, and TBL-SoundEffect with Huzaifa's note data) are documented separately in the software-tables part of Section 5.*

### TBL-Tilemap

One row per cell, per bank. Backed by the two 12,288-byte regions at offsets `0x0000–0x2FFF` (bank 0) and `0x3000–0x5FFF` (bank 1) in the register map.

| Field | Type | Width | Constraint |
|---|---|---|---|
| bank_id | UINT8 | 1 bit (0 or 1) | Part of PK; identifies which tilemap bank |
| cell_x | UINT8 | 7 bits | Part of PK; 0–127 |
| cell_y | UINT8 | 7 bits | Part of PK; 0–95 |
| tile_index | UINT8 | 8 bits | FK → TBL-TilePattern.tile_index; 0–255 |

- **PK:** (bank_id, cell_x, cell_y)
- **Row count:** 2 banks × 128 × 96 = 24,576 rows (12,288 bytes/bank)
- **Used by:** SR-HAL-03, SR-HAL-04, SR-HAL-08, SR-REN-01, SR-REN-03, SR-REN-07, SR-REN-08

### TBL-TilePattern

One row per tile design. Backed by the 8 KB region at offset `0x6000–0x7FFF`.

| Field | Type | Width | Constraint |
|---|---|---|---|
| tile_index | UINT8 | 8 bits | PK; 0–255 |
| pattern_data | BLOB (4-bit packed) | 32 bytes | 8×8 pixels, 4 bits/pixel, 2 pixels/byte (low nibble = left pixel); each nibble is a palette_index FK → TBL-Palette.palette_index |

- **PK:** tile_index
- **Row count:** 256 (8,192 bytes total)
- **Used by:** SR-REN-04, SR-REN-06

### TBL-Palette

One row per colour slot. Backed by the 64-byte region at offset `0x8000`.

| Field | Type | Width | Constraint |
|---|---|---|---|
| palette_index | UINT8 | 4 bits | PK; 0–15 |
| rgb | UINT32 | 32 bits | Packed as `0x00RRGGBB`, 8 bits per channel (matches the ADV7123 DAC's 8 wired bits/channel, CON-06) |

- **PK:** palette_index
- **Row count:** 16 (64 bytes total)
- **Used by:** SR-REN-05, SR-REN-06

### TBL-RegisterMap

One row per addressable register (the control/status/ID/swap registers only — not the bulk memory regions already covered above). Holds the same rows as the register map in 3.1.4, which is the normative definition; this table models them as data.

| Field | Type | Width | Constraint |
|---|---|---|---|
| offset | UINT32 | 32 bits | PK; byte offset within the 2 MB LW bridge window |
| name | VARCHAR(32) | — | Unique; e.g. "Control Register", "Swap Request / Front Bank" |
| access | ENUM (R, W, RW) | — | Per-bit access may differ (e.g. Swap Request/Front Bank is W on write, R on read of a different meaning) — noted in description |
| width_bits | UINT8 | 8 bits | Register width in bits |
| reset_value | UINT32 | 32 bits | Value on power-on/reset |
| description | VARCHAR(256) | — | What the register does |

- **PK:** offset
- **Row count:** 8 (Control, Status, Frame Counter, Swap Request/Front Bank, ID/Version, Tone Frequency, Tone Duration, Audio Control)
- **Used by:** SR-HAL-01, SR-HAL-02, SR-HAL-05, SR-HAL-06, SR-HAL-07, SR-HAL-08, SR-HAL-09, SR-HAL-10, SR-HAL-11, SR-HAL-12, SR-REN-03
