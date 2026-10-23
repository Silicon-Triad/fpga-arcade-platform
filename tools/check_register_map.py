#!/usr/bin/env python3
import re
import sys
from pathlib import Path

def parse_int(val_str: str) -> int:
    val_str = val_str.strip().replace("_", "")
    if "'" in val_str:
        # e.g. 32'hA2CADE01 or 16'h0000
        parts = val_str.split("'")
        if len(parts) != 2:
            raise ValueError(f"Invalid literal format: {val_str}")
        base_char = parts[1][0].lower()
        num_part = parts[1][1:]
        base = {'h': 16, 'd': 10, 'b': 2, 'o': 8}.get(base_char, 10)
        return int(num_part, base)
    elif val_str.lower().startswith("0x"):
        return int(val_str, 16)
    else:
        return int(val_str, 0)

def parse_sv_pkg(path: Path) -> dict:
    content = path.read_text()
    constants = {}
    # Match parameter int FOO = ...; or parameter FOO = ...;
    pattern = re.compile(r'parameter\s+(?:int\s+)?([A-Z0-9_]+)\s*=\s*([^;]+);')
    for match in pattern.finditer(content):
        name = match.group(1)
        expr = match.group(2).strip()
        try:
            constants[name] = parse_int(expr)
        except Exception as e:
            raise ValueError(f"Failed to parse SV parameter {name} = '{expr}' in {path}: {e}")
    return constants

def parse_cpp_header(path: Path) -> dict:
    content = path.read_text()
    constants = {}
    # Match constexpr ... FOO = ...;
    pattern = re.compile(r'constexpr\s+[\w:]+\s+([A-Za-z0-9_]+)\s*=\s*([^;]+);')
    for match in pattern.finditer(content):
        name = match.group(1)
        expr = match.group(2).strip()
        # Clean up C++ type suffixes or casts if any (e.g. U, L)
        expr_clean = re.sub(r'\b[uUlL]+\b', '', expr).strip()
        try:
            constants[name] = parse_int(expr_clean)
        except Exception as e:
            raise ValueError(f"Failed to parse C++ constant {name} = '{expr}' in {path}: {e}")
    return constants

def verify_maps(cpp_path: Path, sv_path: Path) -> list:
    cpp_map = parse_cpp_header(cpp_path)
    sv_map = parse_sv_pkg(sv_path)
    
    errors = []
    
    # Define mapping between C++ name and SV name where they differ, or expect matching normalized keys
    # Let's map key equivalents
    key_pairs = [
        ("kBridgeBaseAddress", "BRIDGE_BASE_ADDRESS"),
        ("kBridgeWindowSize", "BRIDGE_WINDOW_SIZE"),
        ("kOffsetIdVersionReg", "OFFSET_ID_VERSION_REG"),
        ("kOffsetControlReg", "OFFSET_CONTROL_REG"),
        ("kOffsetStatusReg", "OFFSET_STATUS_REG"),
        ("kOffsetAudioControlReg", "OFFSET_AUDIO_CONTROL_REG"),
        ("kOffsetAudioStatusReg", "OFFSET_AUDIO_STATUS_REG"),
        ("kOffsetAudioDataReg", "OFFSET_AUDIO_DATA_REG"),
        ("kOffsetSwapReqFrontBankReg", "OFFSET_SWAP_REQ_FRONT_BANK_REG"),
        ("kOffsetTilemapBaseReg", "OFFSET_TILEMAP_BASE_REG"),
        ("kOffsetTilePatternBaseReg", "OFFSET_TILE_PATTERN_BASE_REG"),
        ("kOffsetPaletteBaseReg", "OFFSET_PALETTE_BASE_REG"),
        ("kOffsetTimerReg", "OFFSET_TIMER_REG"),
        ("kOffsetIrqMaskReg", "OFFSET_IRQ_MASK_REG"),
        ("kResetIdVersionReg", "RESET_ID_VERSION_REG"),
        ("kResetControlReg", "RESET_CONTROL_REG"),
        ("kResetStatusReg", "RESET_STATUS_REG"),
        ("kResetAudioControlReg", "RESET_AUDIO_CONTROL_REG"),
        ("kResetAudioStatusReg", "RESET_AUDIO_STATUS_REG"),
        ("kResetAudioDataReg", "RESET_AUDIO_DATA_REG"),
        ("kResetSwapReqFrontBankReg", "RESET_SWAP_REQ_FRONT_BANK_REG"),
        ("kResetTilemapBaseReg", "RESET_TILEMAP_BASE_REG"),
        ("kResetTilePatternBaseReg", "RESET_TILE_PATTERN_BASE_REG"),
        ("kResetPaletteBaseReg", "RESET_PALETTE_BASE_REG"),
        ("kResetTimerReg", "RESET_TIMER_REG"),
        ("kResetIrqMaskReg", "RESET_IRQ_MASK_REG"),
        ("kHalIdMagic", "HAL_ID_MAGIC"),
        ("kGridCols", "GRID_COLS"),
        ("kGridRows", "GRID_ROWS"),
        ("kTilemapBankSize", "TILEMAP_BANK_SIZE"),
        ("kTilePatternCount", "TILE_PATTERN_COUNT"),
        ("kBytesPerTilePattern", "BYTES_PER_TILE_PATTERN"),
        ("kTilePatternRamSize", "TILE_PATTERN_RAM_SIZE"),
        ("kPaletteSize", "PALETTE_SIZE"),
        ("kBytesPerPaletteEntry", "BYTES_PER_PALETTE_ENTRY"),
        ("kBitControlEnable", "BIT_CONTROL_ENABLE"),
        ("kBitControlReset", "BIT_CONTROL_RESET"),
        ("kBitStatusFpgaReady", "BIT_STATUS_FPGA_READY"),
        ("kBitStatusVblank", "BIT_STATUS_VBLANK"),
        ("kBitStatusAudioFull", "BIT_STATUS_AUDIO_FULL"),
        ("kBitAudioControlEnable", "BIT_AUDIO_CONTROL_ENABLE"),
        ("kBitSwapRequest", "BIT_SWAP_REQUEST"),
        ("kBitFrontBank", "BIT_FRONT_BANK"),
        ("kPaletteRedShift", "PALETTE_RED_SHIFT"),
        ("kPaletteGreenShift", "PALETTE_GREEN_SHIFT"),
        ("kPaletteBlueShift", "PALETTE_BLUE_SHIFT"),
        ("kPaletteMask", "PALETTE_MASK"),
        ("kPixelsPerByte", "PIXELS_PER_BYTE"),
        ("kLeftPixelNibbleShift", "LEFT_PIXEL_NIBBLE_SHIFT"),
    ]
    
    for cpp_key, sv_key in key_pairs:
        if cpp_key not in cpp_map:
            errors.append(f"Missing in C++: {cpp_key}")
            continue
        if sv_key not in sv_map:
            errors.append(f"Missing in SV: {sv_key}")
            continue
        if cpp_map[cpp_key] != sv_map[sv_key]:
            errors.append(f"Mismatch for {cpp_key} ({cpp_map[cpp_key]}) vs {sv_key} ({sv_map[sv_key]})")
            
    return errors

def main():
    root = Path(__file__).resolve().parent.parent
    cpp_header = root / "sw" / "hal" / "register_map.hpp"
    sv_pkg = root / "hw" / "rtl" / "arcade_regs_pkg.sv"
    
    print("self-test: running register map parity check...")
    try:
        errs = verify_maps(cpp_header, sv_pkg)
        if errs:
            print(f"check_register_map: {len(errs)} mismatch(es) found:")
            for e in errs:
                print(f"  - {e}")
            sys.exit(1)
        print("check_register_map: OK, C++ and SV register maps are fully synchronized.")
    except Exception as e:
        print(f"check_register_map error: {e}", file=sys.stderr)
        sys.exit(1)
        
    # Test bad fixture
    print("self-test: checking violating fixture...")
    bad_fixture_cpp = root / "tools" / "fixtures" / "register_map_bad" / "sw" / "hal" / "register_map.hpp"
    bad_fixture_sv = root / "tools" / "fixtures" / "register_map_bad" / "hw" / "rtl" / "arcade_regs_pkg.sv"
    if bad_fixture_cpp.exists() and bad_fixture_sv.exists():
        try:
            bad_errs = verify_maps(bad_fixture_cpp, bad_fixture_sv)
            if bad_errs:
                print(f"self-test: violating fixture correctly failed with {len(bad_errs)} mismatch(es).")
            else:
                print("self-test ERROR: violating fixture passed unexpectedly!", file=sys.stderr)
                sys.exit(1)
        except Exception as e:
            print(f"self-test: violating fixture triggered error/mismatch as expected: {e}")

if __name__ == "__main__":
    main()
