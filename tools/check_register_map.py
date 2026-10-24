#!/usr/bin/env python3
"""tools/check_register_map.py

Parity checker for the two frozen copies of the register map
(sw/hal/register_map.hpp and hw/rtl/arcade_regs_pkg.sv), per
SRS §3.1.4 / TBL-RegisterMap and AGENTS.md section 6 (issue #4).

Exit 0 only if every named constant that appears in BOTH files
(matched by name -- kFooBar in C++ is paired with FOO_BAR in SV,
no hand-maintained list) has the same integer value, and no
constant is unique to one file. Any mismatch, missing counterpart
or unparseable literal is a hard failure, never a silent skip.
"""
import argparse
import re
import shutil
import sys
import tempfile
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
CPP_PATH = REPO_ROOT / "sw/hal/register_map.hpp"
SV_PATH = REPO_ROOT / "hw/rtl/arcade_regs_pkg.sv"

CPP_CONST_RE = re.compile(
    r"\b(?:inline\s+)?constexpr\s+[\w:<>]+\s+([A-Za-z_][A-Za-z0-9_]*)\s*=\s*([^;]+);"
)
SV_CONST_RE = re.compile(
    r"\blocalparam\s+(?:bit\s*\[[^\]]+\]\s+|logic\s*\[[^\]]+\]\s+|int\s+)?"
    r"([A-Za-z_][A-Za-z0-9_]*)\s*=\s*([^;]+);"
)


def parse_int_literal(raw: str) -> int:
    """Parse a C++ or SystemVerilog integer literal, underscores and
    width prefixes/suffixes included. Raises ValueError on anything
    that isn't a clean integer literal -- callers must treat that as
    a hard error, never a skip."""
    s = raw.strip()

    # SystemVerilog sized literal: <width>'<base><digits>, e.g. 32'h0000_0004
    m = re.match(r"^\d*'([hHdDbBoO])([0-9a-fA-F_xXzZ?]+)$", s)
    if m:
        base = {"h": 16, "d": 10, "b": 2, "o": 8}[m.group(1).lower()]
        digits = m.group(2).replace("_", "")
        return int(digits, base)

    # C++: strip underscores, C++14 digit separators ('), and integer
    # suffixes (u/U/l/L in any order/repetition), e.g. 0x0020_0000u
    s_clean = s.replace("_", "").replace("'", "")
    s_clean = re.sub(r"[uUlL]+$", "", s_clean)
    if re.match(r"^0[xX][0-9a-fA-F]+$", s_clean):
        return int(s_clean, 16)
    if re.match(r"^[+-]?\d+$", s_clean):
        return int(s_clean, 10)

    raise ValueError(f"unrecognised integer literal: {raw!r}")


def extract_constants(path: Path, pattern: re.Pattern) -> dict:
    text = path.read_text()
    out = {}
    for name, expr in pattern.findall(text):
        try:
            out[name] = parse_int_literal(expr)
        except ValueError as exc:
            # An unparseable constant is an error, never a silent skip --
            # a skip is exactly how a real mismatch slipped through before.
            raise ValueError(f"{path}: cannot parse constant {name!r} = {expr!r}: {exc}") from exc
    if not out:
        raise ValueError(f"{path}: no constants matched -- check the regex against this file")
    return out


def normalise_name(name: str) -> str:
    """Map a C++ kPascalCase (or exact-SRS UPPER_SNAKE) name and an SV
    ALL_CAPS name to the same comparison key, so e.g. kOffsetControlReg
    and OFFSET_CONTROL_REG line up without a hand-maintained pair list."""
    s = re.sub(r"^k(?=[A-Z])", "", name)            # drop a leading 'k' prefix
    s = re.sub(r"([a-z0-9])([A-Z])", r"\1_\2", s)    # camelCase word boundaries
    s = re.sub(r"([A-Z]+)([A-Z][a-z])", r"\1_\2", s)  # e.g. IDVersion -> ID_Version
    return re.sub(r"_+", "_", s).upper().strip("_")


def compare(cpp_consts: dict, sv_consts: dict):
    cpp_by_key, sv_by_key = {}, {}
    for name, value in cpp_consts.items():
        cpp_by_key.setdefault(normalise_name(name), []).append((name, value))
    for name, value in sv_consts.items():
        sv_by_key.setdefault(normalise_name(name), []).append((name, value))

    mismatches, only_cpp, only_sv = [], [], []
    for key, cpp_entries in cpp_by_key.items():
        if key not in sv_by_key:
            only_cpp.extend(n for n, _ in cpp_entries)
            continue
        for cname, cval in cpp_entries:
            for sname, sval in sv_by_key[key]:
                if cval != sval:
                    mismatches.append((cname, cval, sname, sval))
    for key, sv_entries in sv_by_key.items():
        if key not in cpp_by_key:
            only_sv.extend(n for n, _ in sv_entries)
    return mismatches, only_cpp, only_sv


def run(cpp_path: Path, sv_path: Path) -> int:
    cpp_consts = extract_constants(cpp_path, CPP_CONST_RE)
    sv_consts = extract_constants(sv_path, SV_CONST_RE)
    mismatches, only_cpp, only_sv = compare(cpp_consts, sv_consts)

    ok = True
    if mismatches:
        ok = False
        print("MISMATCHES (value differs between C++ and SV):")
        for cname, cval, sname, sval in mismatches:
            print(f"  {cname} = {cval:#x}  !=  {sname} = {sval:#x}")
    if only_cpp:
        ok = False
        print(f"Only in {cpp_path}, no SV counterpart: {sorted(only_cpp)}")
    if only_sv:
        ok = False
        print(f"Only in {sv_path}, no C++ counterpart: {sorted(only_sv)}")

    if ok:
        print(f"OK: {len(cpp_consts)} C++ constants, {len(sv_consts)} SV constants, "
              f"all matched and equal.")
        return 0
    return 1


def run_selftest() -> int:
    """(1) the real files must currently pass; (2) a copy of the real
    C++ file with exactly one value corrupted must be caught. No
    on-disk fixture to drift -- the bad case is generated here."""
    print("--- self-test: real files must currently pass ---")
    if run(CPP_PATH, SV_PATH) != 0:
        print("SELF-TEST FAILED: the real files do not currently match")
        return 1

    print("--- self-test: an injected mismatch must be caught ---")
    needle = "0xA2CADE01"
    real_text = CPP_PATH.read_text()
    if needle not in real_text:
        print(f"SELF-TEST FAILED: expected to find {needle} in {CPP_PATH} to corrupt")
        return 1

    with tempfile.TemporaryDirectory() as tmp:
        bad_cpp = Path(tmp) / "register_map.hpp"
        bad_cpp.write_text(real_text.replace(needle, "0xA2CADE02", 1))

        try:
            result = run(bad_cpp, SV_PATH)
        except ValueError as exc:
            # A parser exception here is a self-test failure, not a pass --
            # the earlier version of this script got this backwards.
            print(f"SELF-TEST FAILED: parser raised instead of reporting a mismatch: {exc}")
            return 1

        if result == 0:
            print("SELF-TEST FAILED: the injected mismatch was not caught")
            return 1
        print("Injected mismatch correctly caught (the failure above is expected).")

    print("--- self-test: all checks passed ---")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--selftest", action="store_true",
                         help="run the self-test suite instead of checking the real files")
    parser.add_argument("--cpp", type=Path, default=CPP_PATH)
    parser.add_argument("--sv", type=Path, default=SV_PATH)
    args = parser.parse_args()
    return run_selftest() if args.selftest else run(args.cpp, args.sv)


if __name__ == "__main__":
    sys.exit(main())
