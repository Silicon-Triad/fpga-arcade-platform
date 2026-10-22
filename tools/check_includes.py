#!/usr/bin/env python3
"""Layering check for sw/ (QA-01, AGENTS.md R-4.1.9).

QA-01: Game -> Arcade API -> HAL -> FPGA registers; no layer calls a layer above it or
bypasses the layer below it. Enforced on #include lines:
  - only files under sw/hal/ may include register_map.hpp;
  - files under sw/games/ may include only arcade/*.hpp, C++17 standard-library headers,
    and their own game's headers (files inside the same sw/games/<game>/ folder).

Every run first checks the violating fixture in tools/fixtures/includes_bad/ and fails if
the check does not catch every violation there, so CI proves the check can fail. The
output lists every violation as file:line: reason (the include-dependency report).

Usage: tools/check_includes.py [--root DIR]
"""
import argparse
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
FIXTURES = REPO / "tools" / "fixtures"

SOURCE_SUFFIXES = {".cpp", ".cc", ".hpp", ".h"}
INCLUDE_RE = re.compile(r'^\s*#\s*include\s*([<"])([^>"]+)[>"]')

# C++17 standard-library headers (ISO/IEC 14882:2017, [headers]), including the <cxxx> forms
# of the C headers. The <xxx.h> forms are not allowed; the message names the <cxxx> form.
STD_HEADERS = {
    "algorithm", "any", "array", "atomic", "bitset", "cassert", "ccomplex", "cctype",
    "cerrno", "ciso646", "cstdalign", "cstdbool", "ctgmath",
    "cfenv", "cfloat", "charconv", "chrono", "cinttypes", "climits", "clocale", "cmath",
    "codecvt", "complex", "condition_variable", "csetjmp", "csignal", "cstdarg",
    "cstddef", "cstdint", "cstdio", "cstdlib", "cstring", "ctime", "cuchar", "cwchar",
    "cwctype", "deque", "exception", "execution", "filesystem", "forward_list",
    "fstream", "functional", "future", "initializer_list", "iomanip", "ios", "iosfwd",
    "iostream", "istream", "iterator", "limits", "list", "locale", "map", "memory",
    "memory_resource", "mutex", "new", "numeric", "optional", "ostream", "queue",
    "random", "ratio", "regex", "scoped_allocator", "set", "shared_mutex", "sstream",
    "stack", "stdexcept", "streambuf", "string", "string_view", "strstream",
    "system_error", "thread", "tuple", "type_traits", "typeindex", "typeinfo",
    "unordered_map", "unordered_set", "utility", "valarray", "variant", "vector",
}

# Expected violations in tools/fixtures/includes_bad/, as (file, included header).
EXPECTED_BAD = {
    ("sw/engine/bad_engine.cpp", "hal/register_map.hpp"),
    ("sw/games/bad_game/bad_game.cpp", "hal/register_map.hpp"),
    ("sw/games/bad_game/bad_game.cpp", "engine/engine.hpp"),
    ("sw/games/bad_game/bad_game.cpp", "SDL2/SDL.h"),
    ("sw/games/bad_game/bad_game.cpp", "../other_game/other.hpp"),
    ("sw/games/bad_game/bad_game.cpp", "arcade/../engine/engine.hpp"),
    ("sw/games/bad_game/bad_game.cpp", "stdint.h"),
    ("sw/games/bad_game/after_string.cpp", "hal/register_map.hpp"),
}
TOKEN_RE = re.compile(r'//|/\*|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'')


def includes(path):
    """Yield (line number, delimiter, header) for each #include, skipping block comments."""
    in_comment = False
    for n, line in enumerate(path.read_text(encoding="utf-8", errors="replace").splitlines(), 1):
        if in_comment:
            if "*/" not in line:
                continue
            line = line.split("*/", 1)[1]
            in_comment = False
        # Drop comments left to right, keeping string and character literals, so neither
        # "// see arcade/*.hpp" nor const char* s = "/*"; opens a block comment.
        code = ""
        while line:
            m = TOKEN_RE.search(line)
            if not m:
                code += line
                break
            code += line[: m.start()]
            if m.group()[0] in "\"'":
                code += m.group()
                line = line[m.end():]
                continue
            if m.group() == "//":
                break
            end = line.find("*/", m.end())
            if end < 0:
                in_comment = True
                break
            line = line[end + 2:]
        m = INCLUDE_RE.match(code)
        if m:
            yield n, m.group(1), m.group(2).strip()


def game_include_error(root, path, delim, header):
    """QA-01: games see only the Arcade API, the standard library and their own files.

    Returns the reason a game may not include header, or None if it may."""
    error = "games may include only arcade/*.hpp and the standard library"
    if ".." in Path(header).parts and header.startswith("arcade/"):
        return error + " (no '..' in arcade/ paths)"
    if header.startswith("arcade/") and header.endswith(".hpp"):
        return None
    if delim == "<":
        if header in STD_HEADERS:
            return None
        c_form = "c" + header[:-2]
        if header.endswith(".h") and c_form in STD_HEADERS:
            return error + f" (use <{c_form}>)"
        return error
    game_dir = root.joinpath(*path.relative_to(root).parts[:3])  # sw/games/<game>
    target = (path.parent / header).resolve()
    # Only a file that really exists in this game's folder; otherwise the compiler
    # finds it on the include path (sw/), i.e. in another layer.
    if target.is_file() and target.is_relative_to(game_dir.resolve()):
        return None
    return error


def check(root):
    """Return the list of (relative file, line, header, reason) violations under root/sw."""
    violations = []
    sw = root / "sw"
    for path in sorted(sw.rglob("*")):
        if path.suffix not in SOURCE_SUFFIXES or not path.is_file():
            continue
        rel = path.relative_to(root).as_posix()
        in_hal = rel.startswith("sw/hal/")
        in_game = rel.startswith("sw/games/") and len(Path(rel).parts) > 3
        for n, delim, header in includes(path):
            if Path(header).name == "register_map.hpp" and not in_hal:
                violations.append((rel, n, header, "only sw/hal/ may include register_map.hpp"))
            elif in_game:
                error = game_include_error(root, path, delim, header)
                if error:
                    violations.append((rel, n, header, error))
    return violations


def self_test():
    """Prove the check fails on the violating fixture and passes on the clean one."""
    found = {(f, h) for f, _, h, _ in check(FIXTURES / "includes_bad")}
    ok = True
    if found != EXPECTED_BAD:
        print("self-test FAILED: violating fixture not caught as expected")
        for f, h in sorted(EXPECTED_BAD - found):
            print(f"  missed:     {f}: {h}")
        for f, h in sorted(found - EXPECTED_BAD):
            print(f"  unexpected: {f}: {h}")
        ok = False
    for f, n, h, _ in check(FIXTURES / "includes_good"):
        print(f"self-test FAILED: clean fixture flagged {f}:{n}: {h}")
        ok = False
    if ok:
        print(f"self-test: violating fixture fails with {len(found)} violations, "
              "clean fixture passes")
    return ok


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--root", type=Path, help="check this tree instead (no self-test)")
    args = ap.parse_args()

    if args.root is None and not self_test():
        return 1
    root = (args.root or REPO).resolve()
    violations = check(root)
    for f, n, h, reason in violations:
        print(f"{f}:{n}: #include {h}: {reason} (QA-01)")
    if violations:
        print(f"check_includes: {len(violations)} violation(s)")
        return 1
    print("check_includes: OK, no layering violations in sw/ (QA-01)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
