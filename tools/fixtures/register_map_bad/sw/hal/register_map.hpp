// FROZEN INTERFACE: see AGENTS.md section 6. Do not change without an approved issue.
#pragma once

#include <cstddef>
#include <cstdint>

namespace arcade::hal {

// SRS §3.1.4, SR-HAL-01: Base address and address window for AXI/Avalon-MM bridge
inline constexpr std::uintptr_t kBridgeBaseAddress = 0xFF200000;
inline constexpr std::size_t kBridgeWindowSize = 0x10000;

// SRS §3.1.4, TBL-Tilemap: Tilemap dimensions and bank size
inline constexpr std::size_t GRID_COLS = 128;
inline constexpr std::size_t GRID_ROWS = 96;
inline constexpr std::size_t TILEMAP_BANK_SIZE = 12288;

// SRS §3.1.4, TBL-TilePattern: Tile pattern RAM parameters
inline constexpr std::size_t TILE_PATTERN_COUNT = 256;
inline constexpr std::size_t BYTES_PER_TILE_PATTERN = 32;
inline constexpr std::size_t TILE_PATTERN_RAM_SIZE = 8192;

// SRS §3.1.4, TBL-Palette: Palette parameters and pixel packing (0x00RRGGBB, low nibble = left pixel)
inline constexpr std::size_t PALETTE_SIZE = 16;

// SRS §3.1.4, TBL-RegisterMap: Register offsets and hardware reset values
inline constexpr std::uint32_t OFFSET_TILEMAP_BANK0    = 0x0000;
inline constexpr std::uint32_t RESET_TILEMAP_BANK0    = 0x00000000;

inline constexpr std::uint32_t OFFSET_TILEMAP_BANK1    = 0x3000;
inline constexpr std::uint32_t RESET_TILEMAP_BANK1    = 0x00000000;

inline constexpr std::uint32_t OFFSET_TILE_PATTERN_RAM = 0x6000;
inline constexpr std::uint32_t RESET_TILE_PATTERN_RAM = 0x00000000;

inline constexpr std::uint32_t OFFSET_PALETTE_BASE     = 0x8000;
inline constexpr std::uint32_t RESET_PALETTE_BASE     = 0x00000000;

inline constexpr std::uint32_t OFFSET_CONTROL_REG      = 0x8040;
inline constexpr std::uint32_t RESET_CONTROL_REG      = 0x00000000;

inline constexpr std::uint32_t OFFSET_STATUS_REG       = 0x8044;
inline constexpr std::uint32_t RESET_STATUS_REG       = 0x00000002; // FPGA_READY default

inline constexpr std::uint32_t OFFSET_FRAME_COUNTER    = 0x8048;
inline constexpr std::uint32_t RESET_FRAME_COUNTER    = 0x00000000;

inline constexpr std::uint32_t OFFSET_SWAP_REQ_FRONT   = 0x804C;
inline constexpr std::uint32_t RESET_SWAP_REQ_FRONT   = 0x00000000;

inline constexpr std::uint32_t OFFSET_ID_VERSION       = 0x8050;
inline constexpr std::uint32_t RESET_ID_VERSION       = 0xA2CADE01;

inline constexpr std::uint32_t OFFSET_TONE_FREQUENCY   = 0x8054;
inline constexpr std::uint32_t RESET_TONE_FREQUENCY   = 0x00000000;

inline constexpr std::uint32_t OFFSET_TONE_DURATION    = 0x8058;
inline constexpr std::uint32_t RESET_TONE_DURATION    = 0x00000000;

inline constexpr std::uint32_t OFFSET_AUDIO_CONTROL    = 0x805C;
inline constexpr std::uint32_t RESET_AUDIO_CONTROL    = 0x00000000;

// SRS §3.1.4, SR-HAL-09: Expected magic ID value
inline constexpr std::uint32_t HAL_ID_MAGIC            = 0xA2CADE01;

// SRS §3.1.4, Control Register bits
inline constexpr int BIT_CONTROL_RESET_TILEMAP      = 1;
inline constexpr int BIT_CONTROL_DISPLAY_ENABLE     = 2;

// SRS §3.1.4, Status Register bits
inline constexpr int BIT_STATUS_VSYNC_FLAG          = 0;
inline constexpr int BIT_STATUS_FPGA_READY          = 1;
inline constexpr int BIT_STATUS_SWAP_PENDING        = 2;

// SRS §3.1.4, Audio Control Register bits
inline constexpr int BIT_AUDIO_CONTROL_ENABLE       = 0;
inline constexpr int BIT_AUDIO_CONTROL_MUTE         = 1;

// SRS §3.1.4, Swap Request / Front Bank bits
inline constexpr int BIT_SWAP_REQ_FRONT_TRIGGER     = 0;

} // namespace arcade::hal
